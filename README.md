# 数据库系统概论，THU 2020秋

在Ubuntu 16上运行暂未出现差错，windows 10上可以编译，但是文件读写有问题，无法正常运行。

## 页式文件系统

直接使用提供的源文件。参看filesystem目录下源代码。

## 记录管理模块

参看record目录下KontoRecord.cpp/h。实现了记录管理模块的类主要是 KontoTableFile 类。
在一个文件中存储一个表，其中第一页记录各种元信息，第二页和之后为记录的存储。具体存储方式在 KontoRecord.cpp 里有所说明。

##

/*
第 0 页，
    第0个uint是key数量（单属性索引为1，联合索引大于1）
    接下来每三个uint，是keytype，keypos，keysize
接下来的所有页面：（第 1 页是根节点）
    第0个uint为子结点个数
    第1个uint为该节点类型，1为内部节点，2为叶节点
    第2个uint为上一个兄弟节点页编号（若不存在则为0）
    第3个uint为下一个兄弟节点页编号
    第4个uint为父节点页编号
    第5,6,7个uint为keytype，keypos，keysize
    若为内部节点，从第64个uint开始：每（1+keysize）个uint，
        第0个uint为子节点页编号
        后keysize个int为该子节点中最小键值
    若为叶子节点，从第64个uint开始，每（2+keysize）个uint存储
        对应记录的page和id，和该键值
*/

## 注

我习惯在自己项目的文件、类中都加上一个前缀表示这是我自己写的项目。这次用的是 Konto。为了纪念让我痛苦和沉沦的这个秋天，用上DECO*27「弱虫モンブラン」中的「コントラクト会議、あたしはまだ君の中に落ちていくの」中的 Konto。