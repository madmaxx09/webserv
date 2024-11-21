# webserv
A simple webserver
To use: 
- git clone this repo
- cd into it
- make
- run with no args will launch the default.conf
- else use another config

we have a simple website to test some functionnalities, open a browser and connect with http://localhost:8000

A simple webserver in Cpp++
We extensively read the RFC before, and while coding the project. which definitely is a whole new world when never having done network related programming before.
The request parsing process and the configurations parsing and handling is inspired by the nginx source code
For the project we could chose between poll and select for I/O multiplexing. We went with select because we found it easier but we know that for scalebility poll performs better not to mention epoll()

This webserv will only accept GET POST and DELETE requests sent with HTTP, it manages file upload and download, chunked and multiform request
It supports CGI's in python, php, shell, there are a few examples on the website

