# Web Console 前端

Personal Music Center（`docs/web-console/`）。

```bash
pnpm install
pnpm dev          # http://localhost:5173 ，/api 代理到 127.0.0.1:12120
pnpm build        # 输出 dist/，由 LocalServer 静态托管
pnpm typecheck && pnpm test
```

`./build.sh -b` 会自动 `pnpm build`，并把 `dist/` 装入
`MusicPlayer.app/Contents/Resources/local-server/`，由 LocalServer
（默认 `http://127.0.0.1:12120/`）静态托管。

Debug 开发也可在 `MusicPlayer.ini` 设置 `LocalWWW` 指向本目录的 `dist/`。
