# 2020秋数据库系统概论大作业
> 计83 刘轩奇 2018011025

接口说明参看各头文件。

[comment]: 提交项目总结报告，内容包括：系统架构设计，各模块详细设计，主要接口说明，实验结果，小组分工，参考文献。

## 1 模块设计

主要包括页式文件系统模块、记录管理模块、索引模块、用户终端模块。页式文件
系统系统模块负责读写文件，为上层结构提供页式文件操作服务。记录管理模块
负责管理维护数据表，包括创建、删除、插入、查询、修改等。索引模块提供
索引功能加速记录管理模块的查询功能。用户终端模块用于与用户交互，解析用户命令
并调用相关模块进行处理。

### 1.1 页式文件系统模块

本课程提供了原始的页式文件系统，但是实际使用起来略显不畅，因而使用了从github上找到的另一份页式文件系统系统代码。

> https://github.com/Harry-Chen/SimpleDB/tree/master/src/io

提供的主要功能有：

* 打开文件，以文件描述符（编号）的形式提供访问权限。
* 以8KB页为单位读取文件，维护已读取页面的缓存，标记脏页并在关闭文件或更换页面时写回。

### 1.2 记录管理模块

#### 1.2.1 功能

记录管理模块负责维护数据表文件，提供的主要功能有：

* 指定列定义创建空表。列定义支持整数、浮点数、定长字符串、日期四种格式，并支持默认值和可空属性。
* 读取表文件。
* 插入数据并检验数据的有效性，包括主键限制、外键限制、非空限制等。
* 删除数据。
* 修改数据并检验数据有效性。
* 添加、删除或修改列定义。创建或删除主键。添加或删除外键。
* 通过调用索引模块，创建单列索引、多列索引等。
* 根据单列条件或双列条件查询表内结果。若查询条件符合已定义的索引或主键，则调用索引模块加速查询，否则进行线性遍历查询。
* 对某表的多个查询结果求交。
* 删除表。

#### 1.2.2 表文件存储格式

* 第一页存储元信息，包括
  * 文件名
  * 列定义：类型、长度、名称、是否可空、默认值
  * 已有页数
  * 记录总数（总行数，包括已删除的），实际记录总数（不包括已删除的）
  * 每条记录的长度
  * 列数量
  * 当前最后一页已有的行数
  * 主键：主键列数，各列编号
  * 外键：外键数量，每个外键的列数、列编号、指向的表名、指向的表对应列名
* 其后各页存储数据行
  * 每行存储记录编号`rid`，删除标记，以及实际数据

### 1.3 索引模块

#### 1.3.1 功能

索引模块负责维护索引文件，其功能被记录管理模块所调用。索引文件使用B+树数据结构存储。索引模块提供的主要功能有：

* 根据表的单列或多列创建索引。
* 向索引中添加条目或从索引中删除条目。
* 查询，包括等值查询和区间查询。
* 删除索引。

#### 1.3.2 索引文件存储格式

* 第一页存储元信息，包括
  * 键列数，单属性索引仅一列，联合索引则有多列
  * 页面数量
  * 各列的类型、在数据行存储中的偏移量、列大小
* 其后各页维护B+树结构，每页表示一个B+树节点
  * 第二页为B+树根节点。
  * 每页存储该节点子节点个数、是否叶节点、前驱节点页号、后继节点页号、父节点页号
  * 若为内部节点，则存储每个子节点的页编号和最小键值
  * 若为叶节点，则存储其中每个项在数据表中的位置和对应键值。

### 1.4 用户终端模块

#### 1.4.1 功能

用户终端模块负责与用户交互，部分简单功能（例如切换数据库等）直接由其实现，而与数据表相关的操作则调用记录管理模块工作。其提供的主要功能有：

* 从用户终端输入或文件输入读取命令并执行。包括基础SQL数据库命令、数据表命令、查询命令等，以及部分调试用指令。具体的指令参看附表。
* 为用户输入的错误指令提供错误信息。

#### 1.4.2 查询策略

* 对于 delete 和 update 语句，where 子句仅对单表进行查询。
  * 首先尝试合并比较条件，例如可以将 ` val > a AND val < b ` 合并为 ` a < val < b `
  * 接着判断所有比较项中是否有在对应列上定义了索引，若有一个比较项中定义了索引则将该比较条件作为初始，否则任选一个比较条件作为初始。
  * 进行初始查询，然后对剩下的所有比较项在初始查询的结果中逐个进一步查询。
* 对于 select 语句，where 子句可能进行跨表查询。
  * 首先找到所有非跨表查询，它们可以视为分别在多个表上进行的单表查询，按照以上已经描述的方法对每个表进行单表查询。
  * 循环遍历所有单表查询结果的组合（这就是拼接操作），并判断跨表查询条件。
  * 满足条件者成为查询结果的一项。

## 2 实验结果

成功通过各项基础功能测试，多表连接至少可连接四个表。连接更多表时运行速度较慢但仍可以得出正确结果。

## 3 小组分工

仅由本人一人完成。

## 4 参考文献

页式文件系统参考自
> https://github.com/Harry-Chen/SimpleDB/tree/master/src/io

## 5 接口以及使用方法说明

接口说明参看各个头文件中的代码注释。

使用方法：根目录下 `make` 编译程序到 `build/ktdb.out`，执行此文件即可。

## 6 附：支持指令表

### 6.1 数据库指令

* `alter table <tbname> add constraint <fkname> foreign key (<cols...>) references <ftable> (<fcols...>)` 添加外键。
  * `tbname` 创建外键的表名。
  * `fkname` 外键名。
  * `cols` 指定为外键的列名，以逗号分隔。
  * `ftable` 外键指向的表名。
  * `fcols` 指向的表中对应的列名，以逗号分隔。
* `alter table <tbname> add constraint <pkname> primay key (<cols...>)` 添加主键。
  * `tbname` 创建外键的表名。
  * `pkname` 主键名。实际上该参数没有实际作用，每个表至多仅有一个主键，指定主键名无意义。要求用户输入主键名仅仅为了匹配SQL语法。
  * `cols` 指定为主键的列名，以逗号分隔。
* `alter table <tbname> add index <idname> (<cols...>)` 创建索引。
  * `tbname` 要创建索引的表名。
  * `idname` 索引名。
  * `cols` 索引列在表中的列名，以逗号分隔。
* `alter table <tbname> add primary key (<cols...>)` 创建主键。
  * `tbname` 创建外键的表名。
  * `cols` 指定为主键的列名，以逗号分隔。
* `alter table <tbname> add <coldef>` 添加列。
  * `tbname` 要修改的表名。
  * `coldef` 列定义。其格式见后文，下同。
* `alter table <tbname> drop foreign key <fkname>` 删除外键。
  * `tbname` 要删除外键的表名。
  * `fkname` 要删除的外键名。
* `alter table <tbname> drop index <idname>` 删除索引。
  * `tbname` 要删除索引的表名。
  * `idname` 要删除的索引名。
* `alter table <tbname> drop primary key` 删除主键。
  * `tbname` 要删除主键的表名。
* `alter table <tbname> drop <colname>` 从表中删除某列。
  * `tbname` 要删除列的表名。
  * `colname` 要删除的列名。
* `alter table <tbname> change <coldef>` 修改某列定义。
  * `tbname` 要修改的表名。
  * `coldef` 列定义。其中列名应为表中已有的列名。
* `alter table <tbname> rename <oldcol> <newcol>` 修改某列列名。
  * `tbname` 要修改的表名。
  * `oldcol` 原列名。
  * `newcol` 新列名。
* `alter table <tbname> rename to <newtbname>` 数据表重命名。
  * `tbname` 原表名。
  * `newtbname` 新表名。
* `create database <dbname>` 创建数据库。
  * `dbname` 数据库名。
* `create index <idname> on <tbname> (<cols...>)` 创建索引。
  * 同 `alter table <tbname> add index <idname> (<cols...>)`
* `create table <tbname> (<coldefs...>)` 创建表。
  * `tbname` 表名。
  * `coldefs` 列定义，以逗号分隔。
* `delete from <tbname> where <whereclause>` 从表中删除满足条件的行。
  * `tbname` 表名。
  * `whereclause` 条件。
* `desc <tbname>` 显示某表的元信息。
  * `tbname` 表名。
* `drop database <dbname>` 删除数据库。
  * `dbname` 数据库名。
* `drop index <idname>` 删除某索引。
  * `idname` 索引名。
* `drop table <tbname>` 删除表。
  * `tbname` 表名。
* `insert into <tbname> values (<values...>)` 向表中插入数据。
  * `tbname` 表名。
  * `values` 指示要插入的数据，为用括号表示的多元组的逗号连接
    * 元组的元素应和要插入的表的列定义个数、类型吻合。
* `insert into <tbname> values from <tblfilename>` 从文件中读取并向表中插入数据。
  * `tbname` 表名。
  * `tblfilename` 字符串表示的tbl文件路径名，其路径为绝对路径或相对于数据库系统可执行文件的相对路径。
* `quit` 退出系统。
* `select <*|cols> from <tables...> [where <whereclause>]` 选择。
  * `*|cols` 要选择的列，使用 `*` 表示选择所有列。
  * `tables` 选取自的表。
  * `whereclause` 条件。
* `show database <dbname>` 显示数据库中的可用表信息。
  * `dbname` 数据库名。
* `show databases` 显示可用的数据库。
* `show table <tbname>` 显示数据表信息。
  * 等同于 `desc <tbname>`
* `show tables` 显示当前数据库中可用的表。
* `update <tbname> set <setstmt> where <whereclause>` 修改数据。
  * `tbname` 表名。
  * `setstmt` 赋值句。可以是多个赋值子句的逗号连接，其中赋值子句使用 `<col> = <value>` 的形式。
  * `whereclause` 条件。
* `use [database] <dbname>` 使用数据库。
  * `dbname` 数据库名。
* 列定义格式 `<coldef> = <colname> <type> [not null] [default <value>]`
  * `colname` 列名。
  * `type` 列类型，可使用的有
    * `int` 或 `int(n)` 整数，其中 `n` 指示数据长度。实际上本系统并未实现整数数据长度的功能，支持这个语法仅仅为了匹配SQL语言格式。
    * `char(n)` 或 `varchar(n)` 定长字符串，其中 `n` 指示字符数。
    * `float` 双精度浮点数。
    * `date` 日期。
  * `not null` 可选，指示该列是否可空。
  * `default <value>` 可选，指示默认值。`<value>` 数据格式见下文。
* 数据格式 `<value>`
  * 整数，值在 `-2147483647 ~ 2147483647`
  * 双精度浮点数，支持带小数点的格式但不支持科学计数法表示。
  * 字符串，使用双引号、单引号或反单引号括起。
  * 日期，使用 `Y-M-D` 格式。
* where子句格式
  * 可以是多个单项条件的 `AND` 连接。
  * 条件可以是
    * `<col1> ==|!=|<|>|<=|=> <col2|value>`
    * `<col> is [not] null`
  * 对于 `select` 命令，列名可以以 `<tbname>.<col>` 的方式指定
    * 其中 `<tbname>` 是 `from` 中出现的表。

### 6.2 调试指令
* `debug echo <message>` 向标准输出调试消息。
  * `message` 字符串，表示输出的消息。
* `debug from <tbname> where <whereclause>` 显示调试where子句信息。
  * `tbname` 应用where子句的表。
  * `whereclause` where子句。
* `debug index <idname>` 显示索引调试信息。
  * `idname` 索引名。
* `debug primary <tbname>` 显示主键调试信息。
  * `tbname` 要调试的含有主键的表。
* `debug table <tbname>` 显示数据表调试信息。
  * `tbname` 数据表。
* `debug <message>` 等同 `debug echo <message>`
* `echo <on/off>` 指示命令行模式或文件读入模式。
  * 两者的区别在于是否在每行命令输入之前输出 `>>>` 指示符。
* `echo <message>` 等同 `debug echo <message>`
