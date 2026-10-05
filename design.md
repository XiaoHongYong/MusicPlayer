# 设计

## Player Core
* 播放器都是很成熟的技术，不是核心，设计为支持可以支持各种播放器核心即可，用户可以自由切换。
* 定制统一的接口，可隔离和切换播放器

## LocalServer 前后端通信设计

为了方便开发和使用，播放器的绝大部分管理界面在网页中完成。在播放器中会打开管理网页，然后连接到本地的 WebSocket 接口进行管理.

LocalServer 分为 HTTP Server 和 WebSocket Server 两部分构成。

### HTTP Server
* 提供本地 HTTP 网页管理后台的静态页面

### WebSocket Server
* 提供管理后台的数据接口服务

#### WebSocket 通信加密
原理：

* 采用类似 https 通信的方式，使用 Private Key 和 public key 来交换后期通信使用的 AES key。在连接的时候可需要在播放器中确认连接的客户端。

* 需要防范的网络安全风险
  * 主要是为了防止网络监听引起的通信泄密，进而导致任意的网络连接和控制。

可能的漏洞：
* 中间人攻击
  * 中间人拦截流量，伪造 Public Key 发送给客户端
  * 因为绝大多数的的后台管理都在本地网络中进行，而且会在播放器中有确认的过程，所以可忽略此漏洞。
  * 另外播放器生成的证书也无法采取有效的证书校验过程，所以，暂时也无解。

#### 通信格式
数据都是 uint8 array.
* 0: version
* 1: type
* 2 ~ 5: client id
* 6 ~ n: data

type 类型有：
* MT_GET_PUBLIC_KEY：client 发给 server 获取 public key
* MT_PUBLIC_KEY：server 发给 client，返回 public key
* MT_PUB_KEY_ENC：client 使用 public key 加密的数据
* MT_CLIENT_KEY_ENC：使用 client key 加密的数据

client id 由 server 生成，client 保存并使用.

data 的格式
* 由 type 决定，加密的数据需要解密，否则为 utf-8 的 json 格式.

## 相关文档

* Web Console 完整设计（页面、API/事件契约、数据模型、前端）：见 [docs/web-console/README.md](docs/web-console/README.md)。
* 在现有 C++ LocalServer 上落地 Web Console 的分阶段实施计划：见 [docs/web-console/plan.md](docs/web-console/plan.md)。
