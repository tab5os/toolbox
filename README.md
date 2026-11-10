# Tab5 OS app template

A starting point for a native app for [Tab5 OS](https://github.com/tab5os/tab5os) on the M5Stack Tab5. It is set up to build, and to publish releases from CI.

- `./build.sh` fetches the Tab5 OS SDK and builds the app into `build/<id>.t5app`.
- GitHub Actions runs the same script on every push and pull request, and keeps the `.t5app` as a build artifact.
- Pushing a tag such as `v1.0.0` publishes a GitHub release with the `.t5app` attached.

[Weather](https://github.com/tab5os/weather) and [Music](https://github.com/tab5os/music) are built this way.

## Start a project

1. Click **Use this template** on GitHub, or copy this folder.
2. In `manifest.toml`, set `id` to your own reverse domain name (`com.yourname.yourapp`), and set `name`. The OS tells apps apart by `id`, and an update has to keep it.
3. In `CMakeLists.txt`, rename `hello` in `project()` and `t5_app()`, and list every source file in `SRCS`.
4. Write your app in `main.c`. The [SDK guide](https://github.com/tab5os/tab5os/blob/main/docs/sdk.md), the [API reference](https://github.com/tab5os/tab5os/blob/main/docs/api-reference.md) and the [app developer wiki](https://github.com/tab5os/tab5os/blob/main/docs/wiki/Home.md) cover the rest.

## Build

You need ESP-IDF v6.1 (or just its RISC-V toolchain, `riscv32-esp-elf-gcc` 15.2.0), CMake 3.22+, Ninja, Python 3.11+ and curl.

```sh
./build.sh                       # build/com.example.hello.t5app
```

The first run downloads the SDK version in `tab5os-sdk.txt` from the [Tab5 OS releases](https://github.com/tab5os/tab5os/releases) into `.t5sdk/`, and checks it against the release's `SHA256SUMS`. If `riscv32-esp-elf-gcc` isn't on your `PATH` and `IDF_PATH` is set, the script sources ESP-IDF's `export.sh` itself.

To use an SDK you already have, set `T5_SDK` to its `sdk/` folder: an unpacked release, or `tab5os/sdk` in a checkout with a firmware build.

To install the app, copy the `.t5app` to the SD card and tap it in Files. With a Tab5 in developer mode, the SDK's CLI can also push it, launch it and stream its log:

```sh
.t5sdk/tab5os-sdk-*/tools/t5/t5 pair 192.168.1.50   # once: type the code the Tab5 shows
.t5sdk/tab5os-sdk-*/tools/t5/t5 run .
```

## CI and releases

`.github/workflows/build.yml` builds in ESP-IDF's container image, `espressif/idf:v6.1`, and uploads the package as an artifact named after the app's `id`.

To publish a release, set `version` in `manifest.toml`, commit, and push a matching tag:

```sh
git tag v1.0.0 && git push origin v1.0.0
```

The workflow refuses a tag that doesn't match the manifest's version, so a release always carries the version it claims.

**Signing (optional).** Make a key with `t5 keygen me.pem`, keep it private, and add the file's contents as a repository secret named `T5_SIGNING_KEY`. CI then signs every package, and the package names your key as its publisher. Locally, run `./build.sh --sign me.pem`. `*.pem` files are in `.gitignore`.

**A newer SDK.** Change `tab5os-sdk.txt` to the release you want. Because API level 1 is frozen, an app built for level 1 runs on every later OS, so you only need a newer SDK for functions added after level 1. If a new SDK needs a different ESP-IDF version, change the `container:` image to match. (The template itself follows each Tab5 OS release through `.github/workflows/sync-sdk.yml`. That workflow does nothing outside tab5os/app-template, so you can delete it.)

## License

The template is under the MIT No Attribution licence ([LICENSE](LICENSE)), so your app can use any licence you like.
