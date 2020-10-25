# 数据库系统概论，THU 2020秋

在Ubuntu 16上运行暂未出现差错，windows 10上可以编译，但是文件读写有问题，无法正常运行。

## 页式文件系统

直接使用提供的源文件。参看filesystem目录下源代码。

## 记录管理模块

参看record目录下KontoRecord.cpp/h。实现了记录管理模块的类主要是 KontoTableFile 类。
在一个文件中存储一个表，其中第一页记录各种元信息，第二页和之后为记录的存储。具体存储方式在 KontoRecord.cpp 里有所说明。

## 注

我习惯在自己项目的文件、类中都加上一个前缀表示这是我自己写的项目。这次用的是 Konto。为了纪念让我痛苦和沉沦的这个秋天，用上DECO*27「弱虫モンブラン」中的「コントラクト会議、あたしはまだ君の中に落ちていくの」中的 Konto。