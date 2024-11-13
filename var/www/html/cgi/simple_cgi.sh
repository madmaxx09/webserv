#!/bin/bash

# Print some CGI environment variables
echo "Content-Type: text/plain"
echo ""

echo "CGI Script Executed"
echo "--------------------"
echo "PATH_INFO: $PATH_INFO"
echo "PATH_TRANSLATED: $PATH_TRANSLATED"
echo "QUERY_STRING: $QUERY_STRING"
echo "CONTENT_LENGTH: $CONTENT_LENGTH"
echo "CONTENT_TYPE: $CONTENT_TYPE"
echo "REMOTE_ADDR: $REMOTE_ADDR"
echo "REMOTE_HOST: $REMOTE_HOST"
echo "REQUEST_METHOD: $REQUEST_METHOD"
echo "SERVER_NAME: $SERVER_NAME"
echo "SERVER_PORT: $SERVER_PORT"
echo "SERVER_PROTOCOL: $SERVER_PROTOCOL"
echo "UPLOAD_PATH: $UPLOAD_PATH"

# Provide a basic response
echo ""
echo "Hello from the CGI script!"
