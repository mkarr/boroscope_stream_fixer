# Wifi Borescope/Boroscope Stream Fixer

This tool rewrites the video stream produced by inexpensive wifi borescope/boroscope cameras manufactured by DEPSTECH and others.

# Theory of Operation

### Prior Work

This builds on the work of [Nathan Henrie](https://n8henrie.com/) and [Matthew Plough](https://mplough.github.io/), with further work into reverse engineering the firmware of these inexpensive cameras and decoding the stream format by myself.

A writup on my reverse engineering of the firmware can be found [here](https://mkarr.github.io/20200616_boroscope).

### Stream format

These cameras provide an "augmented" MJPEG stream over port 7060 at IP address 192.168.10.123 when connected to them via wifi. The contents of this augmentation are:

* Start tag ("BoundaryS")
* Frame header
* "Encoded" MJPEG frame
* End tag ("BoundaryE")

The frame header contains frame dimensonal and formatting information, and a frame index, in addition to the frame size in bytes. There are additional field that are currently unknown, but do not affect operation. Additionally, it appears that the dimensional information is often incorrect and doesnt match the MJPEG frame actually sent.

### Frame encoding

The frame encoding is the source of the freezes, corrupted images, and banding through the middle of the image that is seem normally when trying to display the stream. Before being sent across the network, the camera applies a simple inversion on a single byte in the middle of the MJPEG frame. The C code for this is as follows:

```
void video_encode_pass(char *buf,size_t size)
{
  if (0 < (int)size) {
    buf[(int)size / 2] = ~buf[(int)size / 2];
  }
  return;
}
```

Simply inverting this transform produces a clean and perfect stream.

# Building

This tool will build on Linux, macOS, and WSL without additional dependencies. Build by running `make`.

# Usage

`Usage: [-l] [-i file|tcp:addr:port] [-o file|tcp:addr:port]`

This tool can read and write to both file and network streams. By default it takes input from stdin and outputs to stdout.

The input source can be chosen by using the `-i` flag, with an argument specifying the source. The argument must either be a file path, or a string of the format `tcp:<address>:<port>`, with `<address>` and `<port>` replaced with the address and port, respectively.

The output destination can be chosen by using the `-o` flag, with an argument specifying the destination. The argument must either be a file path, or a string of the format `tcp:<address>:<port>`, with `<address>` and `<port>` replaced with the address and port, respectively. For network destinations, you can chose a specific address to bind to, or leave blank to bind to the default interface.

The `-l` flag can be provided to log basic information for processed frames to stderr.

# Examples

Take network stream from camera and rebroadcast a fixed stream:

```
$ ./bfs -i tcp:192.168.10.123:7060 -o tcp::7060
```

Take network stream from camera and dump a fixed stream to disk:

```
$ ./bfs -i tcp:192.168.10.123:7060 -i stream.mjpeg
```

Take a saved stream on disk and pipe to an external media player:

```
$ ./bfs -i stream.mjpeg | ffplayer -f mjpeg -
```

# Known Issues

Windows support is currently lacking. A port is planned.
Tool exits if stream consumer closes the socket/file handle. There is minimal error checking once the stream is actually going. This would be a nice-to-have for the future, but not actively being worked on.
