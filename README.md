# Dembe
## TCP data transporter: single-file, lightweight and fast

Dembe is TCP transporter written in C. It makes two TCP connections (either via listening or connecting) and sends out the data received from the other end and vice versa with its muti-threaded design.
The reason it's single file is that you can easily transfer the file to any machine, build it and use it there. On 64-bit Ubuntu that I tested, compiled executable was 21 KB. 

## Usage

```
usage: dembe <mode> [<TCP_IP1>] <PORT1> <mode> [<TCP_IP2>] <PORT2>

mode
   -l	 Listen mode:  It will listen for a port. This mode doesn't require TCP_IP
   -c	 Connect mode: It will connect to given IP:Port

example: ./dembe -l 8080 -c 10.10.10.40 5000
         This will listen on port 8080 and will connect to 10.10.10.40:5000
```

## Compile
It will compile on Linux/UNIX/MacOS:
`gcc -lpthread dembe.c -o dembe`

On Windows, most probably it will be compiled using [cygwin](https://www.cygwin.com/). I don't have Windows to test it; your feedback will be apprecited.

## License
This project is licensed under the MIT License - see the [LICENSE.md](https://github.com/BloodhoundAllfather/dembe/blob/master/LICENSE) file for details


