# 受限 Paddle Live 100% 折扣验收

这是待用户单独批准的方案，不是已经执行的交易。没有创建折扣、$0 订单、付费订阅或授权行。
Paddle 网站仍待审核时不能执行；也不保证当前账户/支付方式支持零元 Checkout。
执行前核对 Paddle 对该账户的最新验收指引；失败时停止，不自动退回付费。

## 两种权限必须分开

正常 Pro：邮箱验证 + Live 规范客户/订阅 + 获准价格 + 非零 Pro 行 + 真实 captured payment +
可靠付费周期 + 无有效退款/争议。取消后已付费有效周期可以保留；past_due、paused、trialing、
过期均拒绝。普通 completed 零元订单、Webhook Simulation、浏览器成功回调都不能成为正式权益。

本方案例外：只让**管理员明确指定的一个邮箱和一笔具体交易**验证 `pro-foundation` 诊断 gate。
返回 `tier: Pro, reason: live_acceptance_only, authorization_mode: acceptance`，最长两小时，
不能写入 verified_paid_periods，不声称商业功能已交付、不变成正常付费授权。
普通权益返回 authorization_mode=paid 或 none。
例外默认关闭，没有公网申请/生成/延期授权接口。

## 三层限制

1. 配置：AUTH_ENABLED 与 LIVE_ACCEPTANCE_ENABLED 都需确认后才能打开；
   AUTH_TEST_EMAIL_SHA256 必须匹配一个已验证邮箱。没有邮箱限制时整个账号服务拒绝启用例外。
2. 管理员行：migration 0002 的 live_acceptance_grants 钉死 email_hash、customer、subscription、
   transaction、price、discount ID，purpose=paddle_live_acceptance，approved_by 非空，
   approved_at/expires_at 最大两小时，revoked_at 控制即时撤销；一笔交易只有一行。
   不能只用邮箱/customer/subscription ID 创建一般授权。
3. 每次在线证明：Live API 返回的真实 active 客户、active 订阅、completed 交易都匹配，
   交易最近 24 小时内 billed；单个 quantity=1 的 USD2400/月价格，行 subtotal2400、discount2400、
   tax0、total0，grand_total0，无非零 payment attempt。折扣需 100% percentage、recur=false、
   usage_limit=1、times_used=1、只限制此 price，创建至过期不超过 24 小时且交易发生在其有效期内。
   当前订阅周期有效，验收到期取该周期结束和管理员期限较早者。
   已 approved refund/credit 或未 reversed 争议仍拒绝；缺字段或 API 不可用不授予权益。

不能通过 event ID / payload 自定义字段 / simulation ID 前缀猜测真实客户。
只有真实 Live API 记录与人工批准行同时存在，模拟事件本身不生成例外。
价格/货币/税额等如果与要求不同，先停下检查，不降低验证条件以强行通过。

## 单独批准后的执行顺序（本轮不执行）

1. Paddle 网站审核通过；先完成 Resend Verified、只读 key 权限和隔离邮箱送达核验。
   记录当前 Worker version、配置、D1 表/行计数，保留 AUTH_ENABLED=false。
2. 在本地/隔离数据库应用 0002，跑全部回归。得到用户批准后才向 Live D1 加这一张新表、
   部署 bundle；不动旧表、旧记录、webhook endpoint secret 或 Notification Destination。
3. 用户指定唯一验收邮箱，在私下记录中保存规范化邮箱 SHA-256，配置 AUTH_TEST_EMAIL_SHA256。
   该邮箱必须能接收验证码，并是本次 Checkout 的 billing email。不得公示测试优惠码。
4. 用户批准创建受限 Paddle Live 折扣：100%、单次使用、只限 US$24/month 的那个 Live Price、
   有效期不超过 24 小时、**不 recurring**。如后台无法准确设置这些条件，停止，不扩大范围。
   后端只读 key 不创建折扣；通过用户管理后台操作。不在 website 源码塞优惠码或绕过支付字段。
5. 用户自行在官方 Live Checkout 完成该零元验收。提交前查看总额确实为零；有税/非零金额则停止。
   此操作仍会创建真实 Live transaction/subscription，只能在单独批准后进行。
6. 管理员从 Live API/后台核对实际 customer/subscription/transaction/discount/price，不从浏览器
   completed 回调直接授权；确认无非零收款尝试、无退款/争议、折扣条件吻合。
   然后私下向 D1 建立该确切组合的短期批准行，recommended 30 分钟、硬上限两小时。
7. 才经批准启用限定用户认证与例外。Windows --auth login → 邮箱验证码 → --auth status/check；
   gate 200、mode=acceptance、截止时间短期、commercial_feature=false。
   另一邮箱不能完成登录/获得此授权；泄露优惠码最多消耗单次优惠，不授予其他人 Pro。
   原有 paid 客户不参加本次受限接口验收；限制只作用于认证服务，不影响 Free 或 webhook。
8. 验证过期/撤销、退出登录后 token 重放拒绝、网络断开拒绝 Pro/Free 正常。
   即刻撤销批准行并关闭 LIVE_ACCEPTANCE_ENABLED；按用户决定也关闭 AUTH_ENABLED。
9. **取消测试订阅，确认不会发生下个月 US$24 的续费**，归档优惠码。
   recur=false 仅首期折扣，绝不表示永久免费；需要在批准方案中安排取消负责人和时间。
   立即 canceled 使本例外拒绝，测试顺序应先验证成功再取消，不等待续费。
10. 保存无客户资料的结果和版本：交易/订阅真实存在、webhook 2xx、特定 gate 成功及撤销成功。
    零元验收不证明真实非零收款、退款或付费周期保留；这些继续用本地 fixtures，
    未来真实付费测试另行取得用户批准。

Paddle discount usage_limit 是全局次数限制，不是指定邮箱限制，不能单独防止冒领。
本系统的邮箱验证、认证限制、人工交易绑定才是授权边界。
折扣属性依据 [Paddle Get discount](https://developer.paddle.com/api-reference/discounts/get-discount/)。

## 管理员 SQL 示例（占位符；本轮不执行）

仅在核对实际 Live 实体后，把每个占位符替换为私下确认的值。不要在 Git 保存客户/交易信息。
不要拿 webhook simulation 表中 ID 代入，不修改旧订阅表以伪造状态。

```sql
INSERT INTO live_acceptance_grants
 (grant_id,email_hash,customer_id,subscription_id,transaction_id,price_id,discount_id,
  approved_at,expires_at,purpose,approved_by)
VALUES
 ('REPLACE_RANDOM_GRANT_ID','REPLACE_VERIFIED_EMAIL_SHA256',
  'REPLACE_CANONICAL_CUSTOMER_ID','REPLACE_CANONICAL_SUBSCRIPTION_ID',
  'REPLACE_CANONICAL_TRANSACTION_ID','REPLACE_LIVE_MONTHLY_PRICE_ID','REPLACE_SINGLE_USE_DISCOUNT_ID',
  unixepoch(),unixepoch()+1800,'paddle_live_acceptance','REPLACE_OWNER_APPROVAL_REFERENCE');

-- 验收后仅撤销自己的那一行，不删除任何交易或旧记录。
UPDATE live_acceptance_grants SET revoked_at=unixepoch() WHERE grant_id='REPLACE_RANDOM_GRANT_ID';
```

`approved_by` 是内部人工审批记录，不是用户可输入的字段。不要自动批准/延长。
不要以 INSERT OR REPLACE 覆盖既有唯一交易批准行。

## 部署影响、停止和回滚

本轮新增表尚未应用到 Live；新代码尚未部署。未来批准部署需保留最新生产版本和变量快照（不导出秘密），
确认无缺签名 POST 仍为400、GET /仍为200、AUTH_ENABLED=false 时 /v1/*为503。
0002 仅添加例外批准表，旧版本不读取它；代码回滚不删除此表。

立即停止：先关闭 LIVE_ACCEPTANCE_ENABLED 和 AUTH_ENABLED，撤销确切 grant，保留 webhook。
随后按记录的实际先前版本执行 Wrangler rollback；重新确认 flag、env.DB、secret 名称、健康/验签。
回滚不会撤销 D1 schema/变量/secret/支付订阅，要单独确保测试订阅已取消。
不清空 auth_sessions、subscriptions、webhook_events，不运行 destructive/down migration。

## Work 1 gate 契约

接口保持 `/v1/features/pro-foundation/check`，新增 JSON authorization_mode 与
`X-RABBITX-Authorization-Mode`（paid / acceptance / none），原有 tier/valid_until 保持兼容。
这个唯一 gate 是诊断功能，不把 offline MockAI 变成收费能力。
未来任何商业 gate 必须单独确认 feature ID 和产品边界，服务端需要 mode=paid；
不能泛化为“tier=Pro 就能调用所有付费服务”，也不能保存永久 Pro 标志。
验收可扩展到哪些真正商业 feature 属于单独审核事项，本次没有开放。
Work 1 独占 C++ 文件；本 Work 只改 backend 和授权文档。
