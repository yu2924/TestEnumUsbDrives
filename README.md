# TestEnumUsbDrives

<img src="media/screenshot win.png" width="320">
<img src="media/screenshot mac.png" width="320">

This is an experiment to detect removable media, especially USB memory devices, connected to a PC.  
これはPCに接続されているリムーバブルメディア、特にUSBメモリを検出する実験です。  

- detects the appearance and disappearance of disk volumes  
ディスクボリュームの出現と消滅を検出します
- enumerate removable volumes  
取り外し可能なボリュームを列挙します
- for each volume, find the parent disk and any properties that might be useful to display  
それぞれのボリュームについて、親ディスクや表示して役立ちそうなそうなプロパティを見つけます
- eject the parent disk of the volume  
ボリュームの親ディスクを取り出します

## Target OS

- Windows
- macOS

It may work on older versions of the OS since it does not use the latest API.  
最新のAPIを利用しているわけではないので、古いバージョンのOSでも動くかもしれません。

## Requirement

- [JUCE framework 8.0.6](https://juce.com/download/), but more old versions are possible
- C++ build tools: Visual Studio, Xcode, etc.

## How to build

1. Open the .jucer file with the **Projucer** tool.
2. Correct the JUCE module path and properties, add exporters and save.
3. Build the generated C++ projects.

## Written by

[yu2924](https://twitter.com/yu2924)

## License

CC0 1.0 Universal
