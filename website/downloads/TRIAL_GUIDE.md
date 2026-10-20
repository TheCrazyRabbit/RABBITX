# RABBITX v0.9 QA：3天免费支持试用 / Free supported evaluation

Windows 10/11 x64；无需WSL、付费账户、付款或绑卡。三天从确认收包起计72小时，具体起止由双方确认；没有自动续费。软件没有三天技术到期锁，使用权以附带LICENSE.txt为准。/ Windows 10/11 x64. No WSL, paid account, card or payment required. Support runs for 72 hours from confirmed receipt, with no renewal. There is no automatic expiry lock; LICENSE.txt governs software use.

## 核验 / Verify

解压前，在包所在目录运行 / Before extraction, run in the archive directory:

```powershell
Get-FileHash .\RABBITX-v0.9.0-beta-trial.20261020-windows-x64.zip -Algorithm SHA256
```

预期 / Expected: `See the SHA-256 value on https://crazyrabbit.dev/trial/ and the adjacent .sha256 file. The QA source archive has a different hash.`

不一致则停止，联系提供者。不要关闭防病毒或绕过组织安全策略；被阻止时只反馈脱敏错误。/ Stop on a mismatch. Do not disable antivirus or bypass organizational controls; share only sanitized errors if blocked.

## 先跑本地示例 / Start locally

解压，在解压目录打开两个PowerShell窗口。第一个运行 / Extract and open two PowerShell windows in the extracted directory. In the first:

```powershell
powershell -File .\examples\Start-DemoServer.ps1
```

如执行策略阻止脚本，按组织批准流程处理，不修改系统策略；可先读自带样例报告。/ If script policy blocks this, use your approved process or read the supplied sample reports first.

第二个窗口运行 / In the second:

```powershell
.\RABBITX.exe --help
.\RABBITX.exe --config .\examples\rabbitx.example.json --html --output .\trial-local.html
```

打开生成的trial-local.html。Case1预期5请求、4端点、20观察聚为4规则组，确认漏洞0；以包内QA报告解释细节。这是合成数据，不是真实客户案例。/ Open trial-local.html. Case 1 has 5 requests, 4 endpoints, 20 observations in 4 rule groups and zero confirmed vulnerabilities. See the included QA report. This is synthetic data.

Mock仅供离线演示，不连接真实模型。停止本地服务按Ctrl+C。/ Mock triage is offline and uses no live model. Press Ctrl+C in the server window to stop.

## 再考虑真实任务 / Optional authorized task

先确认精确主机、路径、方法、测试窗口、请求预算和停止联系人。首批仅测试环境、自有或明确书面授权；操作者自己运行，维护者不代扫。预算建议单次最多10请求、间隔至少1000ms，或采用资产所有者更严格的限制。每次重新运行都会产生新预算，应累计计数。GET也可能有副作用，避开状态变更接口。不要复制本地示例的0ms间隔到真实目标。

Confirm exact hosts, paths, methods, window, request limits and stop contact first. Use owned or explicitly authorized test environments only; the participant runs the tool. Suggested ceiling: 10 requests per run, at least 1000ms apart, or stricter owner limits. Count repeated runs cumulatively. GET can have side effects: exclude state-changing routes. Never reuse the local demo's 0ms interval on real assets.

从examples/rabbitx.example.json复制配置，按包内docs/CONFIGURATION.md同时修改targetUrl与scope，而非只换URL。真实目标配置需人工复核后运行，本说明不提供可直接误用的公网目标命令。

Copy examples/rabbitx.example.json and update both targetUrl and scope following docs/CONFIGURATION.md. Review the configuration before running a real task.

## 输出与隐私 / Results and privacy

分别记录候选、观察、规则组、人工确认漏洞。无已验证影响和复现记录不能写“确认漏洞”；零发现不能写“系统安全”。报告可能含URL、响应、Cookie或其他敏感内容，不上传公开Issue。默认仅反馈匿名统计与脱敏错误；凭据、原始报告留在自己设备。无私密渠道时只用合成复现。

Keep candidates, observations, rule groups and manually confirmed vulnerabilities separate. Confirmation needs reproducible impact and a human review record. Zero findings does not establish security. Reports may contain sensitive URLs, responses or cookies: keep raw reports and credentials on your device, and send only anonymous counts or sanitized errors. Use synthetic reproduction if no private channel is agreed.

研究安排建议：维护者只记录匿名编号、事件时间和反馈；任何拟接收的额外资料先征得同意。活动结束30天内删除收集到的可识别反馈；是否匿名引用需单独选择，默认不同意。此为本轮拟采用的数据安排，录取时确认，不代表已部署自动删除机制。

Proposed study handling: retain only anonymous IDs, event times and feedback. Agree before sharing any extra data. Delete collected identifiable feedback within 30 days of study completion. Quoting feedback requires separate opt-in, default no. Confirm this arrangement at enrollment; no automated deletion system is claimed.

反馈入口 / Public non-sensitive contact: https://github.com/TheCrazyRabbit/RABBITX/issues

安全故障、限流、超范围、意外敏感数据或授权撤回时立即停止。/ Stop immediately on service trouble, rate limiting, scope issues, unexpected sensitive data or revoked authorization.
