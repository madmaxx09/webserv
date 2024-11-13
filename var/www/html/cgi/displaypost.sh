#!/bin/bash
# Read the body from standard input
body=$(cat)
# Output the HTTP response headers
echo "$SERVER_PROTOCOL 200 OK"
echo "Content-Type: text/html; charset=UTF-8"
echo ""

# Output the HTML
echo "<!DOCTYPE html>"
echo "<html>"
echo "<head>"
echo "    <title>CGI Script Output</title>"
echo "</head>"
echo "<body>"
echo "    <h1>Request Body</h1>"
echo "    <pre>"
echo "$body"
echo "    </pre>"
echo "</body>"
echo "</html>"

