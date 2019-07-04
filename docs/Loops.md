# Event loops & platforms

# Event loops

[boost-asio]: https://www.boost.org/doc/libs/release/libs/asio/ "Boost Asio"
[wx-widgets]: https://www.wxwidgets.org/ "wxWidgets"
[ev]: http://software.schmorp.de/pkg/libev.html
[libevent]: https://libevent.org/
[libuv]: https://libuv.org/
[gtk]: https://www.gtk.org/
[qt]: https://www.qt.io/
[issues]: https://github.com/basiliscos/cpp-rotor/issues

 event loop   | support status
--------------|---------------
[boost-asio]  | supported
[wx-widgets]  | supported
[ev]          | planned
[libevent]    | planned
[libuv]       | planned
[gtk]         | planned
[qt]          | planned

If you need some other event loop, please file an [issue][issues].

# platforms

event loop   | support status
-------------|---------------
linux        | supported
windows      | supported
macos        | unknown (*)

(*) `rotor` should work on macos due to rather minimalistic requirements.
If you succesfully run `rotor` on macos let me know.

## Addinng loop support guide

