# HTTP Server

This is a program to run a HTTP server which responds to a HTTP requests from multiple clients concurrently on the socket provided as an argument when running the program.

Use the following commands to compile and run the file (Replace portno with the port where the socket must be opened)

```
gcc -pthread httpserver.c -o server
./server portno
```

The program is designed to respond to HTTP requests with the following 3 commands - GET, HEAD, POST

The program has the following features implemented:
1) Default Page - If the requested URL contains a path to a directory, program will search for index.html in that directory and respond with the index.html if found. If nothing is found it will return a file not found error
2) Handling Multiple Connections - The program handles multiple simultaneous connections to the server by starting a new pthread each time a new connection is established.
3) Error Handling - The program handles the following errors:  
    - 404 File Not Found: The program will respond with a 404 File Not Found if the file requested cannot be found at the server, either due to a mistake in the file path or because that file doesn't exist. 
    - 400 Bad Request: The program will respond with a 400 Bad Request if the file requested is not of a supported content type or if non html documents are requested in POST requests
    - 500 Internal Server Error: The program will respond with a 500 Internal Server Error for any other errors.
4) Pipelining - The program supports pipelining when the http version is HTTP/1.1 and the Connection value is either Keep-Alive or not present (default value of Connection for HTTP/1.1 is Keep-Alive). Subsequent HTTP requests that arrive within the timeout period of 10 secs will be handled by the same thread.
5) POST - The program returns a html document with the POST data sent by the client appended to the start of the payload of the response inside html, body, pre and h1 tags.
