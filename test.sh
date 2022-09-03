#!/bin/sh

# TODO: Add tests for escaped quotes when (IF) they are supported.
echo '{}' | ./main

# primitives
echo '{"a":null}' | ./main
echo '{"a":0}' | ./main
echo '{"a":100.1234}' | ./main
echo '{"a":true}' | ./main
echo '{"a":false}' | ./main

# strings
echo '{"":""}' | ./main
echo '{" ":" "}' | ./main
echo '{"a":"b"}' | ./main
echo '{"abracadabra":"b"}' | ./main
echo '{"a":"big bad wolf"}' | ./main

# arrays
echo '{"a":[]}' | ./main
echo '{"a":[1,2,3]}' | ./main
echo '{"a":[{}]}' | ./main
echo '{"a":[[[]]]}' | ./main

# objects
echo '{"a":{}}' | ./main
echo '{"a":{"foo":"bar"}}' | ./main
echo '{"a":{"b":{"c":{}}}}' | ./main
echo '{"a":{"b":{"c":[1,2,3]}}}' | ./main

