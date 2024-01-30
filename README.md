# wasm-leveldb
Port of leveldb to WebAssembly with Emscripten.

通过 `Emscripten::Embind` 的一个简单实现.


* 当前并非完整构建工程，仅是从另一个内部项目中提取出来的部分实现代码, 具体构建方式
可以在`wasm`中查看到，需要先拉取leveldb仓库和单独构建`snappy`压缩模块后使用。

> 对外接口可以在leveldb中查看到.


