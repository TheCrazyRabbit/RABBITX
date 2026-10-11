# 授权服务配置与邮件准备

日期：2026-10-11。本轮只完成本地准备。没有注册 Resend、修改 DNS、添加生产密钥、
部署新代码、启用生产授权、发送真实验证码或创建 Paddle 交易。
生产继续保持 AUTH_ENABLED=false；现有 Live webhook 不受本地改动影响。

## 需要用户完成的外部步骤

### 1. Resend：注册、验证域名、发送专用密钥

1. 注册/登录 [Resend](https://resend.com)，进入 **Domains → Add domain**，添加
   `auth.crazyrabbit.dev`。邮件子域名与现有官网、API 分开使用。
2. 打开 Cloudflare → `crazyrabbit.dev` → **DNS → Records → Add record**，逐项填写
   Resend 域名页面实际给出的 DKIM、发送 SPF/MX 等记录，包括实际主机名、类型、值和优先级。
   不猜测公钥、区域或记录值；不要覆盖根域已有 MX/SPF，不修改官网/API 域名路由。
   如果提供 CNAME，按邮件服务要求使用 DNS only；TXT/MX 本身没有代理开关。
3. 返回 Resend 域名页面验证，直到 **Verified**。此模块不需要开启收件功能；
   验证发送域名不等于自动创建一个可收件的邮箱。
4. **API Keys → Create API key**，权限选择 **Sending access**，限定到验证通过的
   `auth.crazyrabbit.dev`，不要给域名管理/全权限。密钥只显示时私下保存。
5. 推荐发件人配置：`Crazy Rabbit <auth@auth.crazyrabbit.dev>`。
   不依赖此地址收取客户回复；客户支持使用现有真实可收件联系方式。
   不开启验证码邮件的点击/打开跟踪。

发信适配器是 `backend/entitlements.js` 的 `sendCode()`，默认使用 Resend HTTPS API，
带超时、禁止重定向和请求幂等键；可通过 createAuthHandler 的 mailer 替换。
验证码八位、十分钟有效，最多五次尝试；数据库存 HMAC，不存明文验证码。
模拟测试注入内存 mailer，不提供公网测试发信/验证码读取接口。
真实送达验证要在隔离 Worker/D1 中向自己的邮箱进行，不能把测试邮件当成正式登录成功。

域名操作依据 [Resend 域名文档](https://resend.com/docs/dashboard/domains/introduction)；
密钥权限依据 [Resend API key permissions](https://resend.com/changelog/new-api-key-permissions)。

### 2. Paddle：只读 Live 服务端 API key

进入 **Paddle Live → Developer tools → Authentication → API keys**。
若当前界面把 Authentication 放在 **Settings**，进入该页面的 **API keys** 标签。
创建新的受限服务端密钥，仅给予：

| 权限 | 用途 |
| --- | --- |
| customer.read | 按已验证邮箱查找规范客户 |
| subscription.read | 查实时状态、客户关联和当前周期 |
| transaction.read | 查 completed 交易、实际收款和付费周期 |
| adjustment.read | 查退款、credit、争议/撤销 |
| discount.read | 严格 100% 折扣验收例外的折扣证明 |

不授予任何 write 权限。使用 scoped Live `pdl_live_apikey_…` 格式；
网页 `live_…` client-side token 不能替代它。模块校验官方 scoped Live key 格式，
仅请求 `https://api.paddle.com`，固定 `Paddle-Version: 1`，禁止重定向。
为密钥设置合理有效期并安排到期轮换；失效/权限不足会拒绝 Pro，不能回退到旧缓存。
普通正式付费核验不需要写权限，也不会创建/修改交易或订阅。

参见 [Paddle API authentication](https://developer.paddle.com/api-reference/about/authentication/)
与 [discount.read](https://developer.paddle.com/api-reference/discounts/get-discount/)。

### 3. Cloudflare：添加新 Secret 与非秘密变量

完成服务注册后进入 **Workers & Pages → rabbitx-backend → Settings → Variables and Secrets**。
这里只增加下列新项，不读取、不修改现有 `PADDLE_LIVE_WEBHOOK_SECRET`。

| 名称 | 类型 | 内容 |
| --- | --- | --- |
| RESEND_API_KEY | Secret | 上述 Resend sending-only key |
| PADDLE_API_KEY | Secret | 上述 Paddle Live read-only server key |
| AUTH_HASH_SECRET | Secret | 独立 CSPRNG 生成的 32 字节随机数，转为 64 位十六进制 |
| AUTH_MAIL_FROM | Text | Verified 域名的发件人 |
| PRO_PRICE_IDS | Text | 已核对的 Live US$24/month Price ID；不是 Sandbox ID |
| AUTH_ENABLED | Text | 此阶段继续 false |
| LIVE_ACCEPTANCE_ENABLED | Text | 此阶段继续 false |

不要在聊天、命令行参数、源码或 Git 中输入密钥。新 API key 不能使用 webhook secret 代替。
用户确认允许配置后，可以从仓库根目录运行安全会话密钥工具：

```powershell
./backend/tools/configure-session-secret.ps1
# 上面默认仅本地说明，无远程操作。以下才会配置 Cloudflare：
./backend/tools/configure-session-secret.ps1 -Apply
```

工具先只检查 secret 名称：AUTH_HASH_SECRET 已存在就停止，不读取/覆盖；不存在才随机生成，
通过标准输入交给 Wrangler，不写入文件、剪贴板或命令历史，不输出值。
需要本机 Node/npm/npx 和已登录 Wrangler。这里不执行 -Apply。
修改密钥可能产生新的 Worker 配置版本，但此工具不部署代码、不打开 AUTH_ENABLED。

`backend/configuration.example.json` 只示范非秘密配置，不自动加载、不部署。
不要把其中占位符视为有效 Price ID。
复制已核对的非秘密变量到 wrangler.jsonc 之前必须核对配置；不要把 Secret 加入 vars。

## 本地配置检查和只读检查

```powershell
node --test backend/tests/*.test.js
node backend/tools/check-readiness.js
```

检查工具默认完全离线，没有任何网络请求，不发送邮件。缺配置时退出码 1 并只显示名称/问题，
不显示密钥。它检查的是 Wrangler 声明的 DB binding，不证明远程 schema 已存在。
本机环境变量里的秘密不会自动从 Cloudflare 下载；不要导出生产 Worker secret。

有独立、私下配置的本机环境变量后，可以**明确选择**只读核验：

```powershell
node backend/tools/check-readiness.js --paddle-read
```

仅进行五项 GET，输出权限名、状态与成功标记，不返回客户/支付信息。
这证明读取权限与列表响应格式，不证明有真实订阅、已核对产品价格或端到端验收成功。
仅 sending 权限的 Resend key 无需域名管理权限；工具不尝试拿它读域名配置。
当前未取得这两种新 key，因此本轮不执行真实 API 读取和送达验证。

## 会话与运行边界

- CSPRNG 会话 token 32 字节；数据库只存 SHA-256 摘要，24 小时过期，注销即撤销。
- HTTPS、无浏览器跨域授权、请求体 4KB 限制、按 IP/邮箱/会话限流。
- 每次 gate 在线读取 Paddle；Webhook receipts 和旧 Pro snapshot 不单独授权。
- 网络/DB/Paddle 不可用返回 503，Pro 拒绝、现有 Free 不变。
- `AUTH_TEST_EMAIL_SHA256` 可把启用后的认证限制为一个规范化邮箱的 SHA-256。
  100% 折扣验收模式必须设置该项；其他邮箱返回相同登录提示但不发信、不创建可用 challenge。
  已有其他邮箱会话也不能继续查权益；注销仍允许。
- 邮箱 SHA-256 是用于匹配的标识，不能替代邮件验证码或秘密；不要公开其对应真实邮箱。
- 本轮未切换任何公共认证接口。详见 [严格零元验收方案](LIVE_ZERO_TOTAL_ACCEPTANCE.md)。

先完成配置和隔离送达/只读检查，再由用户确认是否部署、是否启用限定用户验收。
不能仅因 key 格式正确、健康检查 200 或模拟测试通过就开放商业授权。
