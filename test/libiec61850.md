# libiec61850客户端中connectionHandlingThread用于管理客户端/服务器连接的生命周期

- 这个线程的主要职责包括：

    - 管理 TCP 连接（如 MMS 客户端与服务器的通信）
    - 检测连接状态（心跳检测、超时判断）
    - 断线自动重连（指数退避策略）
    - 资源清理（关闭无效连接，释放内存）
    - 事件通知（如连接成功、断开、错误等）


- 根据connectionHandlingThread这个函数功能，实现一个tcp客户端具有这个功能的demo为`tcp_client_connection.c`

# 单向链表`linked_list.h`

- 链表结构
```
struct sLinkedList{
    void* data;
    struct sLinkedList* next;
};
```

- 单向链表特点:

    - 只有next指针, 没有prev指针
    - data是void*, 可以存储任何类型的数据
    - 使用场景61850库中大部分使用场景都是顺序遍历，很少需要反向查找
    - 对于顺序访问，单向链表性能足够好
    - 插入/删除主要在末尾
    - 不需要随机访问
    - 内存敏感(嵌入式系统)

- 双向链表的特点:
    - 需要反向查找
    - 频繁的中间插入/删除
    - 需要快速删除当前节点
    - 需要双向迭代
    - 内存不是主要考虑因素

- 本库中主要用于:
    - 存储变量名列表
    - 存储数据集成员
    - 存储文件目录条目
    - 存储报告配置
    - 存储控制对象


# CID文件
CID就是Configured IED Description(已配置的IED描述)
```
SCL (Substation Configuration Language)
├── IED (智能电子设备)
│   ├── AccessPoint (访问点)
│   │   ├── Server (服务器)
│   │   │   ├── LDevice (逻辑设备)
│   │   │   │   ├── LN (逻辑节点)
│   │   │   │   │   ├── DOI (数据对象实例)
│   │   │   │   │   │   ├── DAI (数据属性实例)
│   │   │   │   │   │   └── SDI (子数据对象实例)
│   │   │   │   │   └── ...
│   │   │   │   └── ...
│   │   │   └── ...
│   │   └── ...
│   └── ...
└── ...
```

### MMS路径的含义
`PROT/LLNO$SP$ActSetGrp`

```
<逻辑设备>/<逻辑节点>$<功能约束>$<数据对象>[.<数据属性>...]
```
### SP是功能约束(Functional Constrain, FC)的标识

- 在MMS协议中,功能约束FC是用来区分同一个数据对象在不同用途下的不同"视图"或者属性集合。
- 常见FC有:ST(状态)，MX(测量量), CO(控制),SP(设定点), SG(设置组)，CF(配置)，DC(描述)，SV(服务)，SE(设置)，RP(报告)，BR(缓冲报告)，GO(GOOSE), GS(GSSE), LG(日志)，EX(外部引用)等。
- SP代表Set Point即设定点 设定值 这个路径的含义是在LLNO逻辑节点下，功能约束为SP的ActSetGrp的数据对象
<++>

###  

