## GameMgr——一个轻量级游戏管理与存档同步项目
#### 1.项目介绍
本项目可以为您提供游戏管理——编写配置文件后可使用图形化界面启动游戏，以及存档同步——满足条件的情况下，可以将存档文件上传至服务器，同时在需要时从服务器拉取存档文件，以在多台设备上完成存档的同步。本项目仅提供可执行文件，**不提供**服务器，也**不承担**因本项目造成的任何后果。
#### 2.配置说明
本项目提供两个可执行文件，一个服务端可执行文件（支持Linux/Windows），一个客户端可执行文件。使用时需要将发行包内对应文件夹`svr`（服务端）和`clt`拷贝到目标位置，并根据实际情况修改配置文件，您可能需要修改的有`服务端配置文件的端口` `客户端配置文件的服务器地址/端口，实际游戏目录及存档目录`
#### 3.使用说明
按照上述操作编写好配置文件后，您可以启动客户端，此时您应该可以在右侧滚动列表看到游戏栏目，左侧为状态栏，包含服务器信息，同步线程信息和软件执行操作，若配置正确，则您可以点击右侧游戏栏目以启动游戏，若需进行存档同步，请进行进一步操作。在本机或另一台设备上，拷贝服务端文件夹，您需要预留一定量的磁盘空间存放游戏存档，依靠配置文件配置一个空闲端口给服务端，之后运行服务端可执行文件，若程序未在数秒内（视设备情况，预留给初始化足够时间再进行判断）退出，则服务端启动成功，若未启动成功，您应该优先排查目标端口是否被占用，或根据日志文件`log/log.log`获取更多信息。当服务端成功启动且客户端与服务端网络通畅且正确设置地址与端口号，您可以启动客户端，或点击左侧`启动同步线程`按钮尝试连接服务器，若同步线程未在数秒内退出，则成功连接至服务器。此时启动任意游戏，将会触发增量/差异同步，上传/拉取/删除存档文件，完成同步，并在游戏主进程退出后再次触发同步操作。

#### 4.构建说明
本项目使用CMake构建系统，您需要安装CMake，版本号至少3.12，clone整个仓库后，运行`./Common/SyncLib/SyncLib.py`以拉取第三方库（源码分发的EbbGlow和源码分发的TideEcho），同时您需要使用其他手段获取Raylib 6.0的发行版，并正确配置EbbGow的CMakeLists.txt，使其可以正确链接Raylib，之后，您可以依靠项目根目录下的CMakeLists.txt使用CMake构建该项目。
#### 5.其他
本项目GitHub地址
```URL
https://github.com/linmu-i/GameMgr
```
欢迎各位为本项目提交issue和PR，提出改进意见或共同建设本项目
本项目采用MIT许可证开源
```text
MIT License

Copyright (c) 2026 linmu-i

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```
感谢Raylib，思源黑体，mINI等第三方支持，第三方开源许可证位于`LICENSE_THIRD_PARTY.txt`中