#include <cctype>
#include <cassert>

#include <iostream>
#include <sstream>
#include <exception>
#include <string>
#include <variant>
#include <vector>
#include <map>
#include <unordered_map>

// Use forward decls and define json_array and json_object as
// wrapper structs to avoid circular definition.
struct json_array;
struct json_object;

using json_name  = std::string;
using json_value = std::variant<std::nullptr_t, bool, double, std::string,
				json_array, json_object>;

struct json_array {
  std::vector<json_value> data;
};

struct json_object {
  // unordered_map queries in O(1) time instead of O(log n),
  // but testing is easier when the serialized data is printed
  // out in a deterministic order.
  std::unordered_map<json_name, json_value> data;
};

class json_serializer {
public:
  static std::string serialize(const json_object& obj) {
    json_serializer ser;
    std::visit(ser, json_value(obj));
    return ser.obuf.str();
  }

  // Functor definitions for std::visit() on a json_value,
  // i.e. an alias for a variant.
  void operator()(std::nullptr_t n) { obuf << "null"; }
  void operator()(bool b)           { obuf << (b ? "true" : "false"); }
  void operator()(double d)         { obuf << d; }
  
  void operator()(const std::string& str) {
    obuf << "\"" << str << "\"";
  }

  void operator()(const json_array& arr) {
    if (arr.data.empty()) {
      obuf << "[]"; // legal empty json array
      return;
    }
    
    obuf << "[";
    int n = 0;
    for (auto i : arr.data) {
      json_serializer ser;
      std::visit(ser, i);
      obuf << ser.obuf.str();

      // avoid adding a comma after last element
      ++n;
      if (n < arr.data.size()) { 
	obuf << ",";
      }
    }
    obuf << "]";
  }

  void operator()(const json_object& obj) {
    if (obj.data.empty()) {
      obuf << "{}"; // legal empty json object
      return;
    }
    
    obuf << "{";
    int n = 0;
    for (auto i : obj.data) {
      json_serializer ser;
      std::visit(ser, i.second);
      obuf << "\"" << i.first << "\":" << ser.obuf.str();

      // avoid adding a comma after last element
      ++n;
      if (n < obj.data.size()) { 
	obuf << ",";
      }
    }
    obuf << "}";
  }

private:
  std::ostringstream obuf;
};

class json_parser {
 public:
  json_parser(std::string&& raw_data) : m_raw_data(raw_data) {}
  ~json_parser() {}

  const json_object& get() const { return std::cref(m_obj); }

  void parse() {
    int rpos = m_raw_data.size()-1;
    for (int i=0; i < m_raw_data.size(); ++i) {
      if (m_raw_data[i] == '[') {
	m_obj.data.insert({"", parse_array(0, rpos)});
	return;
      }
    }
    
    m_obj = parse_object(0, rpos);
  }
  
 private:
  static const auto npos = std::string::npos;
  json_parser() = delete;
  json_parser(const json_parser&) = delete;

  json_object parse_object(int pos, /*in/out*/int& rpos) {
    json_object out;
    json_name tmp_name;
    json_value tmp_value;
    int brace_count = 0;
    int cursor = static_cast<int>(npos);
    int tmp = cursor;

    // 1. Parse past the first curly brace, then walk through the buffer
    // to find the terminating "}". There may be child objects, so count
    // additional left-braces as we go.
    // TODO: Naive implementation, makes parsing O(mn) for m characters
    // and n objects + arrays.
    cursor = m_raw_data.find("{", pos);
    rpos = cursor;
    if (cursor == npos) {
      throw std::runtime_error("improperly-closed object; unmatched curly braces");
    }
    ++cursor; // skip over leading curly brace
    ++brace_count;
    
    for (tmp = cursor; tmp < m_raw_data.size(); ++tmp) {
      if (m_raw_data[tmp] == '{') {
	++brace_count;
      } else if (m_raw_data[tmp] == '}') {
	--brace_count;
      }
      
      if (brace_count == 0) {
	break;
      }
    }

    if (brace_count != 0) {
      throw std::runtime_error("improperly-closed object; unmatched curly braces");
    }
    rpos = tmp+1;

    // 2. Iterate through each name and value in the object.
    // ":" separates names from values in each element.
    // "," separates each element.
    while (cursor < rpos-1) {
      tmp_name = parse_string(cursor, /*out*/tmp);
      cursor = tmp;

      tmp = m_raw_data.find(":", cursor);
      if (tmp == npos) {
	throw std::runtime_error("improperly-delimited formatted object element;"
				 " name and value must be separated by \":\"");
      }
      ++tmp; // skip over ":"
      cursor = tmp;

      tmp = rpos-1;
      tmp_value = parse_value(cursor, /*in/out*/tmp);
      cursor = tmp;
      out.data.emplace(std::move(tmp_name), std::move(tmp_value));

      tmp = m_raw_data.find(",", cursor);
      if (tmp == npos) { // parsed last value;
	break;
      }
      ++tmp;
      cursor = tmp;
    }
    
    return out;
  }
  
  json_array parse_array(int pos, /*out*/int& rpos) {
    json_array out;
    json_value tmp_value;
    int brace_count = 0;
    int cursor = static_cast<int>(npos);
    int tmp = cursor;

    rpos = cursor;

    // 1. Parse past the first curly brace, then walk through the buffer
    // to find the terminating "}". There may be child objects, so count
    // additional left-braces as we go.
    cursor = m_raw_data.find("[", pos);
    rpos = cursor;
    if (cursor == npos) {
      throw std::runtime_error("improperly-closed object; unmatched square braces");
    }
    ++cursor; // skip over leading curly brace
    ++brace_count;
    
    for (tmp = cursor; tmp < m_raw_data.size(); ++tmp) {
      if (m_raw_data[tmp] == '[') {
	++brace_count;
      } else if (m_raw_data[tmp] == ']') {
	--brace_count;
      }
      
      if (brace_count == 0) {
	break;
      }
    }

    if (brace_count != 0) {
      throw std::runtime_error("improperly-closed object; unmatched square braces");
    }
    rpos = tmp+1;

    // 2. Iterate through value in the array.
    // "," separates each element.
    while (cursor < rpos-1) {
      tmp = rpos-1;
      tmp_value = parse_value(cursor, /*in/out*/tmp);
      cursor = tmp;
      out.data.emplace_back(tmp_value);
      
      tmp = m_raw_data.find(",", cursor);
      if (tmp == npos) { // parsed the last value
	break;
      }
      ++tmp; // skip over ":"
      cursor = tmp;
    }
    return out;
  }

  // rpos is set to first char after data plus closing brace/quote
  // for strings/arrays/objects
  json_value parse_value(int pos, /*in/out*/int& rpos) {
    json_value out;
    std::string str_val;
    size_t stod_pos = npos;
    
    // Determine the type of the value
    for (int i=pos; i < m_raw_data.size(); ++i) {
      if (m_raw_data[i] == '{') { // object
	out = parse_object(i, rpos);
	break;
	
      } else if (m_raw_data[i] == '[') { // array
	out = parse_array(i, rpos);
	break;
	
      } else if (m_raw_data[i] == '\"') { // string
	out = parse_string(i, rpos);
	break;
	
      } else if (m_raw_data[i] >= '0' &&
		 m_raw_data[i] <= '9') { // number
	// This can throw std::out_of_range if the number is too big or small,
	// but std::invalid_argument will never happen because we parsed at
	// least one numeric character so far. This hard-coded 16 is a hack,
	// but nonetheless correct. An IEEE double-precision float can accurately
	// represent up to 16 decimal digits (inclusive), but iostream and the
	// C std library round to 6 digits. 7 would suffice, but I may come up
	// with something better in the future.
	out = std::stod(m_raw_data.substr(i, 16), &stod_pos);
	rpos = i + stod_pos;
	break;
	
      } else if (m_raw_data[i] == 'n') { // null
	if (i+4 > m_raw_data.size() ||
	    m_raw_data.substr(i, 4).compare("null") != 0) {
	  throw std::runtime_error("failed to parse value, suspected null value");
	}
	rpos = i+4;
	out = nullptr;
	break;
	
      } else if (m_raw_data[i] == 't') { // boolean true
	if (i+4 > m_raw_data.size() ||
	    m_raw_data.substr(i, 4).compare("true") != 0) {
	  throw std::runtime_error("failed to parse value, suspected boolean \"true\"");
	}
	rpos = i+4;
	out = true;
	break;
	
      } else if (m_raw_data[i] == 'f') { // boolean false
	if (i+5 > m_raw_data.size() ||
	    m_raw_data.substr(i, 5).compare("false") != 0) {
	  throw std::runtime_error("failed to parse value, suspected boolean \"false\"");
	}
	rpos = i+5;
	out = false;
	break;
      }
      
    } // for (int i=pos; i < rpos; ++i)
    
    return out;
  }
  
  // TODO: Support escape characters.
  std::string parse_string(int pos, int& rpos) {
    std::string out;
    //int tmp;
    
    int begin = m_raw_data.find("\"", pos);
    if (begin == npos) {
      throw std::runtime_error("string not properly closed with double-quotes");
    }
    ++begin; // skip over leading double-quote 

    rpos = m_raw_data.find("\"", begin);
    if (rpos == npos) {
      throw std::runtime_error("string not properly closed with double-quotes");
    }
    ++rpos; // skip over trailing double-quote
    
    return m_raw_data.substr(begin, rpos-1-begin); // get string without trailing double-quote
  }

  const std::string m_raw_data;
  json_object m_obj;
};

int main(int argc, char** argv) {
  std::ostringstream buf;
  std::string line;
  while (std::getline(std::cin, line)) {
    buf << line;
  }
  
  json_parser parser(buf.str());
  parser.parse();
  
  std::cout << json_serializer::serialize(parser.get()) << std::endl;

  //json_object top_level = parser.get();
  
  return 0;
}
