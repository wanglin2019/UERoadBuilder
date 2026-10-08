# 道路网络车道级拓扑：P1 / P2 设计方案

> 目标：面向「天际线2（CS2）式」路网，补齐当前项目最大缺口——**车道级有向图**。  
> 本方案只新增一个**派生的拓扑层**，不改动既有几何/渲染代码（`ARoadActor` / `URoadLane` / `AJunctionActor` / mesh 逻辑全部保留）。  
> 范围：先设计 P1（车道级拓扑层，承重墙）与 P2（拓扑化路口）。

---

## 0. 现有模型盘点（已核实字段）

| 结构                           | 关键字段                                                                                                                                                           | 用途 / 缺口                             |
| ---------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------- | ----------------------------------- |
| `ARoadActor`                 | `Lanes[]`(URoadLane*)、`BaseCurve`(URoadBoundary*)、`RoadSegments[]`、`HeightPoints[]/HeightSegments[]`、`ConnectedParents[2]`、`ConnectedChildren[]`(FConnectInfo) | 一条连续道路链；**已有车道数组**，但车道间、车道跨路口的连接不存在 |
| `URoadLane : URoadCurve`     | `LeftBoundary/RightBoundary`(URoadBoundary*)、`Segments[]`(FLaneSegment: Dist/LaneType)、`Curve`(FPolyline)、`Length()`                                           | 车道是一等公民，**自带中心线几何**；缺「行驶方向」与「跨路口连接」 |
| `URoadBoundary : URoadCurve` | `LeftLane/RightLane`(URoadLane*)                                                                                                                               | 边界↔车道**双向邻接**已具备（可推出车道左右邻居）         |
| `ELaneType`                  | `None/Driving/Shoulder/Sidewalk/Median/Collapsed`                                                                                                              | 有**类型**无**方向**（上行/下行/禁转）            |
| `AJunctionActor`             | `Gates[]`(FJunctionGate)、`BuildLink()`、`FJunctionSlot::Combine()`                                                                                              | N 向路口已支持；但连接是**道路端→道路端**，**无车道级映射** |
| `FJunctionGate`              | `Road`、`Sign`(+1出/-1进)、`Links[]`(FJunctionLink)、`IsRampOf()`、`GetLinkCurve()`                                                                                  | 能区分进出/匝道；但 Lane 映射靠几何隐式推断           |
| `FJunctionLink`              | `Road`、`InputRoad`、`OutputRoad`、`Radius`                                                                                                                       | 生成的短连接道；**未记录「哪条车道进、哪条车道出」**        |

**结论**：几何与车道（含中心线）都在，缺的是把它们织成**有向连接图**的一层，以及路口里的**车道↔车道显式映射**。

---

## 1. P1 — 车道级有向拓扑层（关键石）

### 1.1 新增数据结构（不改动既有）

```cpp
// 车道端点引用：定位到某条车道（Road + 车道下标 + 端点）
USTRUCT() struct FLaneEndRef {
    GENERATED_USTRUCT_BODY()
    UPROPERTY() ARoadActor* Road = nullptr;
    UPROPERTY() int32 LaneIndex = INDEX_NONE;
    UPROPERTY() uint8  End = 0;            // 0=起点(车道曲线 Dist=0), 1=终点(Dist=Length)
};

UENUM() enum class ELaneTravelDir : uint8 { Forward, Backward, Any };
UENUM() enum class ETurnType : uint8 { Through, Left, Right, Merge, Diverge, UTurn };

// 拓扑节点 = 一个车道端点
USTRUCT() struct FLaneNode {
    FLaneEndRef Ref;
    FVector Pos;            // 端点世界坐标
    FVector Dir;            // 端点切向（按行驶方向）
    ELaneType LaneType;
    bool bIsSink = false;   // 死胡同终点
    bool bIsSource = false; // 路网起点
};

// 拓扑边
USTRUCT() struct FLaneEdge {
    int32 FromNode = INDEX_NONE;
    int32 ToNode   = INDEX_NONE;
    ETurnType Turn = ETurnType::Through;
    uint8 Kind = 0;         // 0=路段内车道(Segment), 1=路口连接(Junction)
    UPROPERTY() UObject* Physical = nullptr; // Segment→URoadLane* ; Junction→FJunctionLink*
    double Length = 0;
    FPolyline Geometry;     // 采样折线，供仿真/绘制
};
```

承载对象（新建，挂在 `ARoadScene` 上，便于随场景加载/保存）：

```cpp
UCLASS() class ROADBUILDER_API URoadNetwork : public UObject {
    GENERATED_BODY()
public:
    void Rebuild(ARoadScene* Scene);              // 全量重建
    void MarkDirty(ARoadActor* Road);             // 增量：标脏
    void MarkDirty(AJunctionActor* Junction);
    void RebuildDirty();                          // 增量重建
    // —— 仿真/AI 查询接口 ——
    const TArray<FLaneEdge>& GetOutgoing(int32 Node) const;
    const TArray<FLaneEdge>& GetIncoming(int32 Node) const;
    bool CanReach(const FLaneEndRef& A, const FLaneEndRef& B) const; // BFS on directed graph
    TArray<FLaneEdge> Route(const FLaneEndRef& A, const FLaneEndRef& B) const;
    int32 FindNode(const FLaneEndRef& Ref) const; // 建表 O(1)
private:
    UPROPERTY() TArray<FLaneNode> Nodes;
    UPROPERTY() TArray<FLaneEdge> Edges;
    TMap<FLaneEndRef, int32> NodeIndex;           // 端点→节点下标
    TArray<TArray<int32>> OutAdj, InAdj;          // 邻接表
    TSet<ARoadActor*> DirtyRoads;
    TSet<AJunctionActor*> DirtyJunctions;
};
```

### 1.2 构建算法 `Rebuild(Scene)`

1. **建节点**：遍历 `Scene->Roads`，每条 `Road` 遍历 `Road->Lanes`，为每个车道建 2 个节点（起点/终点），按 1.3 规则填 `Dir` 与 `bIsSource/bIsSink`。写入 `NodeIndex`。
2. **路段内边（Segment）**：每个车道用一条 `Kind=0` 边连接其起点节点→终点节点（方向见 1.3）。`Physical=该 URoadLane*`。
3. **普通道路接头（非路口）**：对 `ConnectedParents[2]` / `ConnectedChildren`，按车道**横向顺序**配对（左→右第 i 条车道对齐），生成跨接边。这是「无路口」近似连接，路口处会被 1.4 的边覆盖/替代。
4. **路口边（Junction）**：遍历 `Scene->Junctions` → 每个 `FJunctionLink` → 读其车道映射（P2 新增字段，见 §2.2）→ 每条映射生成一条 `Kind=1` 边，`Turn` 由 §1.4 算出，`Physical=该 FJunctionLink*`。
5. **构建邻接表** `OutAdj/InAdj`，供查询。

### 1.3 车道行驶方向解析（关键决策点，需实测验证）

- **假设规则**：以 `BaseCurve` 的 `Dist` 增大方向为道路「正向」。右行车道（`RD_RIGHT=0`，即 `RightBoundary` 在 BaseCurve 右侧）行驶方向 = 正向；左行车道（`RD_LEFT=1`）行驶方向 = 反向；`Shoulder/Sidewalk/Median/None` 为非行驶（`Any` / 不生成行驶边）。
- **验证项 V1**（必须在写实现前做）：检查 `URoadLane::Update(int Side)` / `AddLane(LeftBoundary, RightBoundary, …)` 的实际朝向约定，确认车道 `Curve` 的 `Dist=0` 端落在道路哪一端；若与假设相反，则在 1.2 步骤 2 反转边方向，必要时对 `Curve` 做 `Reverse()`。
- 此规则解析后即可给每条 `FLaneNode` 赋正确 `Dir`，拓扑图才是有向且正确的。

### 1.4 转向类型分类

对一条路口连接边：取**进向车道行驶方向**与**出向车道行驶方向**（或 `FJunctionLink` 几何切向），求有向夹角：

- 夹角 ≈ 0 → `Through`
- 左偏（叉积 > 0）→ `Left`
- 右偏（叉积 < 0）→ `Right`
- `Gate->IsRampOf()` 为真 → `Merge`(汇入) / `Diverge`(分出)，按进出判定

### 1.5 增量重建

`ARoadScene::Rebuild` 与路况变更（编辑 `UpdateCurve`、路口 `Build`）后调用 `MarkDirty`；每帧/每 tick 低频 `RebuildDirty()`，只重建脏道路/脏路口的局部节点与边，重挂邻接。避免天际线式「实时拖动几千段路」时全量重算。

### 1.6 与现有模块的关系（明确**不**动什么）

- **不动**：`ARoadActor` / `URoadLane` / `URoadBoundary` 的几何与 mesh 构建；`AJunctionActor` 的 mesh 生成；编辑器工具链；S4 运行时宿主。
- **只新增**：`URoadNetwork` 派生层 + 在 `ARoadScene` 持有一份引用。所有仿真、AI、寻路、路口生成的「真相源」都读这一层。

---

## 2. P2 — 拓扑化路口（让 P1 的边变正确、变丰富）

### 2.1 Z 净空判定（消除立交误连）⚠️ 当前真实 bug

当前两条路在 XZ 平面相交就建路口；若它们 Z 高度不同（跨线桥/立交），会**错误生成路口**。

- 在 `AJunctionActor::AddRoad` / `AddJunction` 前：对两条 `BaseCurve` 用 `SolveIntersection` 求交点 `S1/S2`；采样 `RoadA->GetHeight(S1)` 与 `RoadB->GetHeight(S2)`；若 `|Δh| > Threshold`（建议 ~5m，按项目单位定）且水平投影确实交叉 → **不建路口**，记为「立体交叉」（可后续挂桥/隧道资源）。
- 此判定同时用于路口内的每条 `FJunctionLink`，保证路口内部也不错误连接不同高程的道路端。

### 2.2 车道↔车道显式映射（改造 `FJunctionLink`）

在 `FJunctionLink` 增加：

```cpp
USTRUCT() struct FLanePair { GENERATED_USTRUCT_BODY()
    UPROPERTY() int32 InLane = INDEX_NONE;   // InputRoad 的车道下标
    UPROPERTY() int32 OutLane = INDEX_NONE;  // OutputRoad 的车道下标
};
// FJunctionLink 内新增：
UPROPERTY() TArray<FLanePair> LaneMap;
```

- `AJunctionActor::BuildLink(Gate, Next, Index)` 在生成短连接道 `Road` 时，**同时**按横向顺序推导并写入 `LaneMap`（进向车道 i ↔ 出向车道 i，按两侧车道横向位置对齐；匝道用 `IsRampOf` 只连一对）。
- 提供**默认自动推导 + 编辑器手动覆盖**（玩家/编辑可改某条转向车道）。
- P1 的 1.2 步骤 4 直接读 `LaneMap` 生成边——从此「看到的线和点到的线、走的车道」三者一致。

### 2.3 方向 / 转向限制 / 单向 / 分隔 / 限速

- **车道方向属性**：`URoadLane` 增加 `ELaneTravelDir TravelDir`（默认由 1.3 规则推导，可覆盖）；非行驶车道 `Any`。
- **转向限制（车道级）**：在 `URoadLane` 上增加 `AllowedTurns` 位掩码（禁左/禁右/仅直行/单向），影响 P1 的 `GetOutgoing` 过滤。这是车道级属性（见 §5.2）。
- **限速（段级，非车道级）**：`SpeedLimit` 挂在 `ARoadActor`（段级），不放在 `URoadLane`；仅「某条车道单独限速」这类覆盖才落到 `URoadLane.SpeedLimitOverride`。依据见 §5.2（OpenDRIVE `RoadInfoSpeed` 为 road 级、CS2 限速按道路类型默认）。
- **分隔式快速路**：用 `Median` 车道 + 双向 `TravelDir` 表达；不做成「两条 ARoadActor 硬拼」，属样式层工作。
- **单向/双向（走廊级）**：由 `bOneWay` 标志或「一个 `ARoadActor` = 单向行车道、双向=两个」表达（见 §5.3）。

### 2.4 路口生成管线重构（为特殊交叉口留接口）

当前 `CornerIndex==1` 当匝道、`IsRampOf` 启发式、硬编码 `Links[CornerIndex]` 偏脆（已知 `Gate.Links[1]` 越界隐患）。

- 抽出一个 `IJunctionGenerator` 接口（或策略类）：`BuildJunction(AJunctionActor*)`；默认实现保持现有几何行为，新增 `RoundaboutGenerator` / `SlipLaneGenerator` / `TurbineGenerator` 可插拔。
- `FJunctionSlot::Combine` 继续负责 mesh 拼接；拓扑层只读其结果（`GetSlots` / `Gates`），不重复逻辑。

### 2.5 顺手清理的隐患（P2 一并做最划算）

- `Gate.Links[1]` 下标越界（已知）→ 改 `IsValidIndex` 判空。
- legacy `AJunctionActor::CurrentTool` 隐式非空裸强转（已知）→ 一并收敛（legacy 保留但清理）。
- `IsRampOf` 的 `#if 0` 死分支 → 清掉，保留角度判定版本。

### 2.6 P2 如何喂给 P1

```
道路几何变更 ──► ARoadScene 标脏 ──► URoadNetwork::RebuildDirty()
                                       │
        P2 的 Z 判定 / LaneMap / 转向限制 作为「边生成规则」注入 1.2 步骤 4
                                       ▼
                          正确的车道级有向图（仿真/AI/寻路真相源）
```

---

## 3. 落地路线 / 里程碑

| 阶段        | 内容                                              | 依赖          | 产出            |
| --------- | ----------------------------------------------- | ----------- | ------------- |
| **P1-M1** | `URoadNetwork` 骨架 + `Rebuild` 全量（仅路段内边 + 普通接头）  | 现有数据        | 单条/连续道路的车道图可查 |
| **P1-M2** | 路口边（读 LaneMap，先占位默认推导） + 邻接表 + `CanReach/Route` | M1, §2.2 占位 | 全路网有向图 + 寻路可用 |
| **P1-M3** | 增量 `MarkDirty/RebuildDirty`                     | M1          | 实时拖动不卡        |
| **P1-M4** | §1.3 方向规则**实测验证(V1)** 并固化                       | M1          | 方向正确          |
| **P2-M1** | Z 净空判定                                          | —           | 立交不再误连        |
| **P2-M2** | `FJunctionLink::LaneMap` 推导 + 写入                | P1-M2       | 车道↔车道映射正确     |
| **P2-M3** | 车道方向 / 转向限制 / 单向                                | P2-M2       | 限制生效          |
| **P2-M4** | `IJunctionGenerator` 管线 + 隐患清理                  | P2-M1~M3    | 特殊交叉口可扩展      |

---

## 4. 风险与验证

- **R1（方向错）**：若 §1.3 假设与实际 `Update()` 相反，全图边方向反转 → 用 V1 在编辑器内取一条双向路，对比车道 `Curve` 端点与道路正向，确认后再固化。
- **R2（车道顺序不稳）**：`Road->Lanes[]` 的横向顺序依赖 `InitWithStyle` 写入次序；增量重建时需以 `Boundary` 的 `LeftLane/RightLane` 邻接为主键而非数组下标，避免插入车道后错位。
- **R3（性能）**：几千车道 × 路口，全量重建 O(N)；靠 M3 增量 + 八叉树（`Scene->Octree` 已存在）做局部脏传播。
- **验证方法**：单测/编辑器内——(a) 一条直路分段接另一条，验证车道图连通；(b) 十字路口验证 4 向 × 车道数 的 Through/Left/Right 边齐全；(c) 立交（不同 Height）验证**不**生成路口；(d) `Route(A,B)` 在含禁转的路口绕行正确。

---

## 5. 属性粒度分层（车道级 vs 道路级：哪些挂哪、路口怎么断）

### 5.1 核心结论：路口断「整条路（段级）」，车道图是派生的逻辑层

回答「路口该断整条道路还是按车道断」：

- **物理/资产层（编辑、存储、渲染的单元）**：在**路口处把道路切成「段」**——两个节点之间的一个行车道（carriageway）就是一段，对应一个 `ARoadActor`。一条路跨两个路口 = 被切成 2 段；两路交叉 = **4 段在路口相遇**（即「4 条独立道路一端成路口」）。车道**不**单独切成 actor，而是作为段的子元素留在段里。
- **拓扑/仿真层（P1 的 `URoadNetwork`）**：在这层**按车道断开**——节点是车道在路口的端点，边是车道↔车道的连接。这是派生图，不是物理 actor。


即：**物理上断到「段」，逻辑上断到「车道」**。这里「段」是拓扑与属性单元，有两种实现（见 §5.5）：要么在路口把 `ARoadActor` 硬切成多个 actor（每个 actor = 一段），要么保留一个 actor、用 `Gate` 的 `Dist` 派生逻辑段。**注意：不要与「每条车道变成独立 actor」混淆**——那是另一回事（车道只是段的子元素，双向 6 车道无需 12 个 actor）。CS2 与 OpenDRIVE 均印证「段=物理/属性单元、车道=拓扑派生单元」：

- **CS2**：道路内部按固定长度切成 **segment**，相连处叫 **node**；车道只是 segment 的属性。车在「下个 node 右转」前会在本 segment 内提前并到右侧车道——拓扑锚点在 node（=段端点），车道连接是段内/跨段属性。
- **OpenDRIVE**：道路有 `laneSection`；跨路口用 **connecting road + `laneLink from/to`** 做**车道↔车道**显式连接，原道路**不**被切成车道级 actor，连接是逻辑 `laneLink`。

### 5.2 属性粒度总表

| 属性                 | 粒度                   | 依据                                                            | 本项目落点                                                       |
| ------------------ | -------------------- | ------------------------------------------------------------- | ----------------------------------------------------------- |
| 单向/双向              | **道路/走廊级**           | 指行车道数（1=单行，2=双向），是走廊属性                                        | `ARoadActor` 标志位，或走廊=2 个 `ARoadActor`（每向一个）                 |
| 道路类型/等级            | 段级                   | 高速/干道/集散                                                      | `ARoadActor` / `URoadStyle`                                 |
| **限速**             | **段级（主）/ 车道级（特例覆盖）** | OpenDRIVE `RoadInfoSpeed` 是 road 级；CS2 限速按道路类型默认、按 segment 上色 | `ARoadActor.SpeedLimit`；可选 `URoadLane.SpeedLimitOverride`   |
| 车道数 / 路面材质 / 宽度    | 行车道级                 | —                                                             | `ARoadActor`                                                |
| **转弯类型 / 转向箭头**    | **车道级**              | 画在某条车道上的转向标线；CS2 可逐车道改转向                                      | `URoadLane.TurnArrows`                                      |
| **每条车道允许转向 / 禁转**  | **车道级**              | 转向运动从某车道发起；OpenDRIVE `laneLink` 车道↔车道                         | `URoadLane.AllowedTurns`（位掩码）                               |
| **标线（车道线类型/颜色）**   | **车道边界级（两车道之间）**     | OpenDRIVE 每条车道定义其左右 `roadMark`，落在分界上                          | `URoadBoundary.Marking`（已具备 `LeftLane/RightLane` 邻接，天然边界宿主） |
| 车道类型（公交/HOV/货车道）   | 车道级                  | —                                                             | `URoadLane.LaneType`（已有）                                    |
| 车道特定限速（货车道慢/可变情报板） | 车道级（覆盖）              | 特例，非常规                                                        | `URoadLane.SpeedLimitOverride`                              |
| 路口类型 / 信号灯 / 路口限速  | 路口级                  | —                                                             | `AJunctionActor`                                            |
| 进向车道↔出向车道映射        | 路口连接级                | OpenDRIVE `laneLink`                                          | `FJunctionLink.LaneMap`（§2.2）                               |

### 5.3 落点澄清（修正此前表述）

- **限速不是车道级基础属性**：标准模型里限速是**段级**；只有「某条车道单独限速」这种**覆盖**才落到 `URoadLane`。见 §2.3 修正。
- **标线挂边界不挂车道**：`URoadBoundary` 已具备 `LeftLane/RightLane` 双向邻接，是承载标线的天然位置，不用在每条车道上各存一份（避免相邻车道重复/冲突）。
- **单向/双向是走廊级**：若 `ARoadActor` 允许双向（车道含两向），加一个 `bOneWay` 标志；若模型规定一个 `ARoadActor` = 单向行车道，则双向走廊 = 两个 `ARoadActor`。

### 5.4 对架构的含义

- 物理段（`ARoadActor`）+ 车道（`URoadLane`）+ 边界（`URoadBoundary`）+ 路口（`AJunctionActor`）四件套**已经能承载全部粒度**，无需新增「车道 actor」。
- P1 的车道图是**纯派生**：从段/车道/边界/路口现有字段重建，不新增物理存储。
- 路口 Chop（段=actor）让「段级属性天然按段独立」，与「4 条独立道路一端成路口」的设想一致。

### 5.5 在路口的两种「断开」实现与推荐

用户提出的两种口径，本质都是「段单元」怎么落（**不是**「每条车道一个 actor」——那是误读，车道只是段的子元素）：

- **A. 硬 chop（actor 级）**：在路口把 `ARoadActor` 切成 N 个 actor，每个 actor = 路口之间的一段。两路交叉 → 长路 A 在 J 处切成 A1/A2、长路 B 切成 B1/B2，共 **4 个 actor 在 J 相遇**。
  - 优点：每段属性（限速/车道构成/道路类型）就是 `ARoadActor` 字段，**零额外机制**即可按段独立；拓扑极简（每个 actor ≤ 2 端、每端一个 `Gate`）；与 CS2/OpenDRIVE「segment/node」范式一致；「4 条独立道路一端成路口」在数据上直接成立。
  - 缺点：路口拖动/增删时要 split/merge actor，actor 身份会变（需稳定 ID）；「这是同一条长路」的连续性需靠轻量 parent/走廊分组维持。
  - 可行性：本项目 `Chop(double Dist)` + `CutLanes`/`CutRoadSegments` **已备**，可直接用。
- **B. 逻辑切分（保留一个 actor）**：`ARoadActor` 不变，靠其 `Gates[]` 的 `Dist` 把路逻辑分成若干区（`[0,D0]`、`[D0,D1]`、`[D1,Len]`）。每段属性用**距离键控属性带**承载（如 `TArray<FRoadAttrBand>{Dist, SpeedLimit, Style}`，或复用中心线的 `FRoadSegment`）。
  - 优点：actor 稳定，拖路口只改 `Gate.Dist`，无 split/merge；天然保留「一条路」连续性。
  - 缺点：属性查询要 band 查找（「dist=350 限速多少？」）；若各区车道构成不同，lane 配置也得按 band 存——基本等于「在 actor 内重造 segment」，比直接多 actor 更复杂；几何（曲线）按区不同还要支持 per-band 曲线，进一步复杂化。
  - **术语纠正**：口语里的「lanesegment 在路口断开」应理解为**道路/中心线级段**（`FRoadSegment` 或 gate 派生段），**不是 `FLaneSegment`**——`FLaneSegment` 是**每条车道自己的类型/截面沿程变化带**（per-lane morph），本就不表示「路口之间的路」；把它当段单元会语义错乱。

**推荐：A（硬 chop）+ 轻量走廊分组**。理由：用户核心诉求是「同一长路跨两路口、想给两段设不同限速/属性」——A 让属性直接是 actor 字段，彻底解决；且 P1 车道图天然以 actor 端为节点。若担心 actor 抖动，可在 actor 上挂 `CorridorId`（或 parent `ARoadActor*` 引用）维持视觉/编辑连续性，二者不矛盾。B 适合「几乎不改路口、又想要稳定 actor」的场景，但代价是属性带机制。

### 5.6 当前实现为何不在路口切开（设计取舍）

代码实证（`RoadScene.cpp`）：`ARoadScene::AddJunction`（:869）只 `SpawnActor<AJunctionActor>` 后两次调用 `Junction->AddRoad(Road, Dist)`；`AJunctionActor::AddRoad`（:154）只按 `Dist` 计算 `FJunctionSlot` 并 `AddGate(Road, Dist, Sign)` 注册一个 `FJunctionGate`——**全程不修改 `ARoadActor` 的 `BaseCurve`/`Lanes`/`RoadSegments`/`HeightPoints`**。所谓「路口切开」只发生在 mesh 层：`FJunctionGate::CutDists[2]`（RoadScene.h:174）在 `AJunctionActor::Update` 里算出，仅用于告诉渲染器「在此处断开画路口」，是几何/渲染关注点，不是逻辑拆分。

即：道路是**连续的、可整体编辑的样条 actor**，路口是**挂在道路某距离上的注解/闸门**，而非对道路的重构。这是典型的「先画线、后交点」草图式编辑器范式。

**不切开的收益**：

1. 单一连贯编辑单元：整条路是一个样条，拖任意控制点整路（含路口附近段）一起动；切开后每段独立 actor，需手动缝合端点。
2. 身份稳定：增/删/移路口都不改变 road actor 身份，undo/选择/存档/外部引用（仿真/AI）始终有效。
3. 路口操作廉价可逆：加/删路口 = 增/删一个 Gate 条目；移路口 = 改 `Gate.Dist` + 重算 `CutDists`。无需 split/merge 几何。
4. 几何只存一份：`BaseCurve`/高程/分段/车道都在一个 actor 上；切开要分裂并维护所有这些数组。
5. N 向连接统一：路口只收集 N 个 `(Road, Dist, Sign)` 闸门，不在乎它们是「段」还是「整路」，`Gates.Sort()`/`Links` 生成逻辑照常工作。
6. 视觉已断开：mesh 按 `CutDists` 在路口处断开，路「看起来」正确连接/切断，无需逻辑拆分。

**代价（正是不切开导致的痛点）**：

- 无段级属性：限速/车道构成/道路类型都是 per-actor，一条跨两路口的长路无法给两段设不同限速。
- 拓扑是隐式的：「两路口之间的段」不是一等对象，要靠 gate 距离派生；P1 车道图可由此派生（不强制切开），但不如显式段对象干净。
- 「4 条路在路口相遇」只是视觉事实，车道↔车道连接是几何推断，非显式映射。

**结论**：不切开对「编辑器」是对的默认（编辑流畅、身份稳定、路口操作廉价）；但它阻断段级属性与显式拓扑。这正是推荐 A（硬 chop）+ 走廊分组的动机——chop 换来段级属性与干净拓扑，走廊 ID 保留「一条路」连续性。注意：**P1 车道图并不要求切开**，两种模型都能从 `Gate.Dist` 派生段；只有当你要把段级属性当一等字段（而非属性带）时，才必须 chop（或走 B 的属性带）。

---

## 6. 结论

- **架构支撑 CS2 式路网**：数据模型（中心线+竖向+多车道+样式+连接+八叉树）地基正确，**无需推倒**，只需「加派生层 + 路口补车道映射」。
- **P1 是承重墙**：把已有车道几何织成有向图，之后仿真/AI/寻路/路口生成全部挂它。
- **P2 让墙立得稳**：Z 感知避免立交误连、车道↔车道显式映射消除「线/点/走不一致」、方向/转向限制是交通规则的载体、生成管线可插拔特殊交叉口。
- 全部为**新增/扩展**，不触碰既有几何与渲染；已知隐患（`Links[1]` 越界、`CurrentTool` 强转、`IsRampOf` 死分支）在 P2 顺手清理。

---

## 7. 人行横道与交通信号设计（L0 数据 / L2 行为分离）

> 本节回答「过街设施 + 信号灯怎么做」。核心约束（已与用户确认）：RoadBuilder 是**几何/数据/编辑器（L0）**，自身**不跑车辆仿真**；人行横道/信号灯的**语义数据在此建、可编辑、可导出**，但「驱动车辆让行/亮灯/减速」发生在下游 **L2 仿真层**。因此 §7.2 的所有实体都是**纯数据定义**，落 L0；L2 读它们决定车让不让、灯怎么亮。RoadBuilder 不实现「把车停下」。

### 7.0 两类横道统一为单一实体（用户结论已采纳）

- 路口内 crosswalk 与路段中间 crosswalk，**车辆行为上应统一**：都要求车遇行人让行、都可能有信号灯（车灯与人灯同相位反相）、都可能有减速带、都在路网里标记一个「过街区」。
- 唯一实质差异 = **信号控制权归属**：路口型 → 引用所属 junction 的 `SignalController`；独立型 → 自带 `SignalController`（或纯让行、无信号）。
- ⇒ 抽象为单一 `URoadCrossing`，差异被一个字段吸收。符合 CS2 实证（路口/路段 crosswalk 同源，仅相位归属不同）。

### 7.1 数据结构（新增，挂 ARoadScene / AJunctionActor）

```cpp
// 7.1.1 信号控制器（路口与过街共用）—— 纯数据定义，运行时状态在 L2
UCLASS() class ROADBUILDER_API URoadSignalController : public UObject {
    GENERATED_BODY()
public:
    UPROPERTY() TArray<FSignalPhase> Phases;     // 相位序列（循环）
    // 当前相位/计时等运行态由 L2 持有，不在这里
};
USTRUCT() struct FSignalPhase {
    GENERATED_USTRUCT_BODY()
    UPROPERTY() FName  Name;
    UPROPERTY() float  Duration = 0;             // 该相位时长(秒)
    UPROPERTY() TArray<int32> VehicleGroupGreen; // 本相位绿灯的车行组(图节点集)下标
    UPROPERTY() bool bPedestrianGreen     = false;// 本相位行人是否可过街
    UPROPERTY() bool bExclusivePedestrian = false;// 行人专用相位(车全红)
};

// 7.1.2 过街设施（统一类型）
UCLASS() class ROADBUILDER_API URoadCrossing : public UObject {
    GENERATED_BODY()
public:
    // —— 几何：横穿短曲线(自身 polyline，不复制道路 curve) ——
    UPROPERTY() FPolyline CrossingCurve;         // 横跨道路的短曲线，供渲染 + 行人路径
    UPROPERTY() float     InfluenceLength = 0;    // 影响长度(停车/让行缓冲)
    // —— 关联：横穿的道路/车道 + 里程区间（引用 + 区间，非几何副本）——
    UPROPERTY() ARoadActor* CrossedRoad = nullptr;
    UPROPERTY() int32      CrossedLane  = INDEX_NONE; // 影响的车道(-1=整段所有车道)
    UPROPERTY() double     DistStart = 0, DistEnd = 0;// 在 CrossedRoad 上的里程区间[S0,S1]
    // —— 行为属性（供 L2 消费，L0 只定义）——
    UPROPERTY() bool bSignalized           = false;   // 是否有信号灯
    UPROPERTY() URoadSignalController* SignalController = nullptr; // ★ 两类唯一差异点
    UPROPERTY() bool bYieldToPedestrian    = true;    // 无信号/行人相位时的让行规则
    UPROPERTY() bool bSpeedBump            = false;   // 减速带
    // —— 视觉 ——
    UPROPERTY() UCrosswalkStyle* CrosswalkStyle = nullptr; // 引用既有斑马线样式资产
    // 两端连 Sidewalk(行人路径基础) 由 CrossingCurve 推导，不另存
};
```

### 7.2 与现有零件衔接（不推翻、只编织）

- `UCrosswalkStyle : UBaseMarkStyle` → 保留作渲染层；Crossing 引用它，沿 `CrossingCurve` `BuildMesh` 画斑马线。
- `URoadProps` → 保留作静态道具层；信号灯杆/标志牌放此处。若要灯随相位变色：扩展 prop 绑信号状态，或 Crossing 直接挂「信号灯 Actor」引用（M3 再做）。
- `ELaneType::Sidewalk` → Crossing 两端的行人路径基础（路侧人行道车道）。
- `AJunctionActor::DebugCrossings`（仅调试蓝点）→ 升级为正式 `TArray<URoadCrossing*> Crossings`，由路口控制器驱动。

### 7.3 几何/数据边界（呼应「图引用几何、不存几何」）

- `CrossingCurve` 是**横穿道路的短曲线**，自身 polyline；它**不复制** `URoadLane::Curve`。
- 车辆定位仍靠被横穿车道的 `Curve`（L0 几何真源）；Crossing 只记录「在 `CrossedRoad` 的 `[S0,S1]` 区间、影响 `CrossedLane`」——是**引用 + 区间**，不是几何副本。
- **不切 road**：road 保持单条连续 `ARoadActor`，lane/boundary 都不断开（避免制造无意义拓扑断点，见 §5.6）。过街要影响车辆时，作为该 segment/lane 的**附加属性**（`bYieldToPedestrian`/`SignalPhase`），而非物理切段。

### 7.3.1 专业做法：crossing 是唯一表示，车道不做"横道段"（已锁定决策）

- **决策**：全部用 `URoadCrossing`，**不在任何 lane / boundary 上纵向截取"横道段"**。路口内与路段中间两类 crosswalk 都如此——即便视觉上想让"每车道在 [S0,S1] 是横道区"，也由 crossing 派生标记，**不把横道塞进 `FLaneSegment` / `FBoundarySegment` 的段列表里**。
- **业界标准一致（crosswalk = 独立对象，按位置引用，绝不插入 driving lane 段）**：
  - **ASAM OpenDRIVE**：crosswalk 是 road 上的 `object` / `signal`（类型 `pedestrianCrossing`），按 `(s, t)` 定位；lane 类型只有 `driving`/`sidewalk`/`bicycle` 等，**无 crosswalk lane type**，车道保持连续。
  - **SUMO**：`<crossing>` 是独立于 `<lane>` 的元素，连接两个 `<walkingArea>`（人行区）并横跨道路，不是车道段。
  - **Lanelet2**：crosswalk 是独立的 `Lanelet`（subtype=`crosswalk`）+ `RegulatoryElement`（路权），与车辆 `Lanelet` **并列的不同图元**，车辆 lanelet **不切开**。
  - **CityGML / IMGeo**：crosswalk 是独立的 `TrafficArea` / 标线要素。
  - **CS2**：内部 `LaneSystem` 在 crosswalk 处给车道做 **cut range**（让车停的缺口）——但那是**构建期派生的仿真产物**（LaneSystem 生成），**作者数据**仍是独立的 crosswalk 对象；车道 cut 不是存储事实。
- **为什么专业做法不在车道里切横道段**：
  1. **本体不同**：lane 编码"车辆可通行性"，crosswalk 编码"行人过街区 + 路权"；把横道塞进 lane type 混淆两类本体。
  2. **单一真源**：横道位置只存一处（`URoadCrossing` 的 `[S0,S1]`），而不是在每条 lane/boundary 各戳一段；移动横道只改一个对象，不会 N 处漂移。
  3. **行为（信号/让行/减速）是道路级**：塞进 per-lane 段会重复 N 份、易不一致。
  4. **几何形态不符**：crosswalk 有自身横向 extent、宽度、斑马纹，不是纵向 lane 段的形状。
  5. **拓扑正交**：车道图里 crosswalk 是"连接两侧行人路径节点"的边，与车行 lane 边正交。
- **唯一可落在 lane / boundary 上的东西**：
  - **停止线**在 `S0` 处——那是 boundary 的*线状标线*（`FBoundarySegment::LaneMarking`），归属正确。
  - （仅仿真派生）车道在 `[S0,S1]` 的*停车预约 / 缺口*（CS2 的 cut range），由 `URoadCrossing` 派生，**绝不作为 lane 段的存储事实**。
- ⇒ 与 §5.6「不切 road、lane 不切段」原则一致，只是把"过街"也纳入"不切"范畴。`ELaneType` 无需加 `Crosswalk`；斑马面走 `UCrosswalkStyle`（面状，非 `FBoundarySegment` 的线状 `LaneMarking`）。


### 7.4 与 P1 车道图的集成（M4）

- Crossing 在 P1 图中表现为：连接道路两侧**行人路径节点**的边（pedestrian edge），并在所横穿的车行 lane 上标记**过街约束**（让行/信号）。
- 过街约束作为 lane 边的属性（或独立的 `TArray<FCrossingConstraint>` 挂在 `URoadNetwork`），供 L2 寻路/AI 读取：遇行人/红灯 → 在 `[S0,S1]` 区间降速/停车。
- 约束是**附加属性**，不是把 lane 图在路口处物理断开。

### 7.5 两类差异落点

- **路口型**：在 junction 上创建 `SignalController`；自动/手动生成 4 个 `URoadCrossing` 挂到它（相位与车行同步）。`AJunctionActor` 持 `SignalController*` + `Crossings[]`。
- **独立型**：`URoadCrossing` 自带一个 `SignalController`（自身即控制器），或 `bSignalized=false` + `bYieldToPedestrian` 仅让行。

### 7.6 编辑/交互草图

- 新工具 `RoadTool_Crossing`（或并入 Marking）：在路面画一条横穿曲线 → 创建 `URoadCrossing`，自动吸附该 road 的车道与里程区间 `[S0,S1]`。
- 详情面板：选 Crossing → 设 `bSignalized / SignalController / bYieldToPedestrian / bSpeedBump / CrosswalkStyle`。
- 路口型：Junction 工具里勾「生成人行横道」，自动在进口创建 Crossing 并归属路口控制器。
- 信号灯杆放置（M3）：在 Crossing 端点放 `URoadProps` 信号杆，绑定控制器相位。

### 7.7 信号相位模型（数据定义，L0）

- `FSignalPhase`：`bPedestrianGreen` / `bExclusivePedestrian`（车全红、行人专用相位；CS2 有 `Exclusive Pedestrian Phase`）。
- 路口型：车行相位与行人相位**同控制器反相**（车绿时人红，车红时人绿/专用）。
- 独立型：自身控制器的相位循环（可纯 `bYieldToPedestrian` 无相位）。

### 7.8 导出（供 L2 消费）

- 内部消费（推荐 M1 起步）：L2 直接持有 `URoadNetwork` / `ARoadScene`，读 `URoadCrossing` + `URoadSignalController` 定义。
- 外部消费（可选）：导出 OpenDRIVE `pedestrianCrossing` 对象 + 信号 `signalHead/controller`；或自定义 JSON（geometry + phase 表）。
- **决策待定**：下游消费形态（a 直接 UObject / b XODR / c 自定义）——决定 M1 字段与是否做导出器。

### 7.9 减速带/让行：属性 vs 独立道路家具（待定）

- MVP 推荐：作为 `URoadCrossing` 的属性（`bSpeedBump` / `bYieldToPedestrian`）——仅 crosswalk 用，简单。
- 未来泛化：抽独立 `URoadFurniture`（停止线/路障/减速带均可复用），多一层但更通用。M1 不做。

### 7.10 落地里程碑

| 阶段 | 内容                                                             | 依赖        | 产出           |
| -- | -------------------------------------------------------------- | --------- | ------------ |
| M1 | `URoadCrossing` 数据 + 渲染斑马线 + 创建/编辑工具（纯数据+视觉）                   | —         | 可放置/编辑过街，可视化 |
| M2 | 行为属性导出（`bYieldToPedestrian`/`bSpeedBump` 供 L2 消费）              | M1        | 下游可实现让行/减速   |
| M3 | `URoadSignalController` + phases；路口型挂 junction 控制器，独立型自建；信号杆绑定 | M1        | 信号灯数据定义完整    |
| M4 | 接入 P1 车道图（行人边 + 过街约束）                                          | P1, M1~M3 | 寻路/AI 可用过街约束 |

### 7.12 完整考量维度（此前未展开的部分）

下面这些维度在前述对话里只点到、未落文档；此处一次性补齐，作为可落地设计的输入。

#### 7.12.1 几何形态细分

- **标准横道 / 分向横道（Split，中间安全岛）**：行人分两段过，路中是 `refuge island`（安全岛）。数据上 = 一个 `URoadCrossing` 但 `Variant=Split`，或拆成两个紧邻的 Crossing 共享安全岛几何。
- **对角横道 / 限时过街（Diagonal / Scramble）**：X 形斜穿，需 `bExclusivePedestrianPhase`（车全红时人才斜穿）。几何仍是两条横穿曲线，但方向与道路呈 45°。
- **抬升式横道（Raised / speed table）**：整体抬高、兼作减速带；`Variant=Raised` 时几何带竖向高程、`bSpeedBump` 隐含 true。
- **对齐方式**：垂直（默认，AlignmentDeg=90）或斜交（angled）；影响斑马条取样方向。
- **横向范围 `[t0,t1]`**：全宽（含自行车道、停车带）vs 仅车行道。用区间而非"整条路"布尔，更精确。
- **立体过街（天桥/地下通道）**：不在 `URoadCrossing` 范畴，属独立 `pedestrian bridge/underpass` 图元；Crossing 只标注地面连接点。
- **几何派生（已确认，随路弯曲）**：两侧边界 `Road.CreatePolyline(S0,S1,t0/t1)`（复用 boundary 同一套 offset 数学），道路一弯横道即弯；斑马条在 `[S0,S1]` 取样、每段 s 连 `GetPos(s,t0)`–`GetPos(s,t1)`。

#### 7.12.2 行人路径网络与拓扑衔接

- Crossing 在**行人图**里是「连接两侧人行道节点」的边；人行道 = `ELaneType::Sidewalk` lane 的行人路径。
- 与 P1 车道图：车行 lane 边挂**过街约束**（让行/信号）作为属性；行人 lane 边里 Crossing 是普通可通行边。两套图共享道路几何、引用同一 `URoadLane`，不双源。
- **跨多条道路的横道**（复杂路口对角）：Crossing 可引用多条 `CrossedRoad`，或直接归属 `AJunctionActor`（由 junction 统一管理 4+ 个进口 Crossing）。
- **环岛横道**：环岛外侧人行过街，归属环岛 junction 的控制器。

#### 7.12.3 行为规则深化（L0 定义、L2 消费）

- **停止线位置 / 缓冲带 `InfluenceLength`**：车停在 `S0` 前，留出排队空间；L2 据此设停车点。
- **行人专用相位 `bExclusivePedestrianPhase`**：车全红、人全绿（scramble 用）。
- **行人按钮 / 感应控制 `bActuated`**：有请求才给相位（数据层记标志 + 请求源类型；运行态 L2 持有计数）。
- **倒计时 / 清场时间**：`FSignalPhase::Duration` 已有，可加 `PedClearanceTime`（行人最后离开时间）。
- **无障碍**：`bTactileStrip`（触感警示带）、`bAudibleSignal`（音响信号）标志位 + 对应视觉。
- **路权优先级（无控制横道）**："车让行人"由 `bYieldToPedestrian` 编码；含支路让主路、环岛内让环岛外等，L2 读规则决策。
- **车辆检测行人（perception）**：纯 L2 职责；L0 只给"过街区"几何与规则。

#### 7.12.4 视觉与标线细分

- **标线样式种类**（扩展 `UCrosswalkStyle` 的 `EMarking` 枚举）：`Zebra / Continental（断续斑马）/ Ladder（梯式）/ Parallel（平行线条）/ Silhouette（行人剪影）`。
- **行人信号灯头（Pedestrian Signal Head）**：红人/绿人小图标，独立子对象或 `TArray<FPedestrianSignalHead>` 挂 Crossing，绑定控制器相位。
- **路缘坡道（Curb Ramp / Dropped Curb）**：横道两端的坡道几何（`bCurbRamp`）。
- **安全岛视觉（Refuge Island）**、**触感警示带视觉**。

#### 7.12.5 横向设施统一抽象（呼应对话结论）

- 基类 `URoadLateralFeature`：定位 `CrossedRoad + [S0,S1] + [t0,t1]`（三类共享）；类型分支：
  - 标线型：`UCrosswalkFeature`（面）/ `UStopLineFeature`（线，横向版，**非** `FBoundarySegment::LaneMarking`）
  - 物理型：`USpeedBumpFeature`（隆起，挡车）
- **停止线正确归属**：横向标线实体（与斑马线平级，都继承 `UBaseMarkStyle`），**绝不进**纵向的 `LaneMarking` 字段（方向正交）。
- **让行标志 = 路侧点状 `URoadSign`**：表达的是规则，与"横跨路面"正交，不进横向设施；牌子视觉走 `URoadProps` 或 `URoadSign`，规则本体（`bYieldToPedestrian` 等）由 Crossing/道路持有。

#### 7.12.6 交互 / 编辑细化

- **吸附**：画横道时自动吸附最近 road、按曲率投影得 `[S0,S1]` 与 `[t0,t1]`。
- **自动识别车道数** → 默认全宽横道（`t0,t1` 取路缘）。
- **路口工具勾选「生成人行横道」** → 自动在 4 个进口创建 Crossing 并归属路口控制器。
- **校验**：横道需基本垂直于道路（角度阈值）、两端须落在可行走边缘（Sidewalk）上，否则提示。

#### 7.12.7 导出格式全覆盖

- **OpenDRIVE**：`<object type="pedestrianCrossing">` + `s/t/length/width` + `outline`（用 road 坐标角点）；信号用 `<signal>` crosswalk 子类型，相位挂路口 controller。
- **SUMO**：`<crossing>` 连两个 `<walkingArea>`、横跨道路。
- **Lanelet2**：crosswalk `Lanelet`（subtype=crosswalk）+ `RegulatoryElement`（right-of-way）。
- **CityGML / IMGeo**：独立 `TrafficArea` / 标线要素。

#### 7.12.8 特殊类型

- **无控制横道**：`bSignalized=false` + `bYieldToPedestrian=true`（仅让行）。
- **学校 / 医院区**：`bYieldToPedestrian` + `bSpeedBump` + `bSchoolZone` 提示标志。
- **共享街道（woonerf）**：整段路设为 shared，横道弱化/不画，靠让行规则。

### 7.13 完整可落地数据模型（扩展字段版）

> 在 §7.1 基础上补齐形态/视觉/行为/无障碍字段；几何仍只存**引用 + 区间**，不存曲线副本（呼应 §7.3）。

```cpp
UENUM() enum class ECrosswalkVariant : uint8 { Standard, Split, Diagonal, Raised };
// UCrosswalkStyle 内新增 ECrosswalkMarking : Zebra, Continental, Ladder, Parallel, Silhouette

UCLASS() class ROADBUILDER_API URoadCrossing : public UObject {
    GENERATED_BODY()
public:
    // —— 几何：引用 + 区间（派生，不存曲线副本）——
    UPROPERTY() ARoadActor*      CrossedRoad = nullptr;
    UPROPERTY() double           S0 = 0, S1 = 0;        // 沿路里程区间
    UPROPERTY() double           T0 = 0, T1 = 0;        // 横向范围(全宽=路缘±半宽)
    UPROPERTY() ECrosswalkVariant Variant = ECrosswalkVariant::Standard;
    UPROPERTY() float            AlignmentDeg = 90.f;   // 与道路夹角(90=垂直)
    // —— 视觉 ——
    UPROPERTY() UCrosswalkStyle* CrosswalkStyle = nullptr; // 含 ECrosswalkMarking
    UPROPERTY() bool bCurbRamp     = true;   // 两端路缘坡道
    UPROPERTY() bool bTactileStrip = true;   // 触感警示带
    UPROPERTY() bool bStopLine     = true;   // 停止线(横向标线, 非 LaneMarking)
    UPROPERTY() UStopLineStyle* StopLineStyle = nullptr;
    // —— 行为（L0 定义, L2 消费）——
    UPROPERTY() bool bSignalized = false;
    UPROPERTY() URoadSignalController* SignalController = nullptr; // ★ 两类唯一差异点
    UPROPERTY() bool bYieldToPedestrian = true;
    UPROPERTY() bool bSpeedBump = false;     // 减速带(或见家具层)
    UPROPERTY() bool bActuated = false;      // 行人按钮/感应
    UPROPERTY() bool bExclusivePedestrianPhase = false; // 专用相位
    UPROPERTY() bool bAudibleSignal = false; // 音响信号(无障碍)
    UPROPERTY() bool bSchoolZone = false;    // 学校/医院区
    // —— 行人信号头（绑定控制器相位）——
    UPROPERTY() TArray<FPedestrianSignalHead> SignalHeads;
};

USTRUCT() struct FPedestrianSignalHead {
    GENERATED_USTRUCT_BODY()
    UPROPERTY() FTransform Pose;            // 红人/绿人图标位置(灯杆上)
    UPROPERTY() int32 PhaseIndex = INDEX_NONE; // 对应 SignalController 的相位
};

// 停止线样式（横向标线，与 UCrosswalkStyle 平级，均继承 UBaseMarkStyle）
UCLASS() class ROADBUILDER_API UStopLineStyle : public UBaseMarkStyle { ... };
```

`URoadSignalController` / `FSignalPhase` 沿用 §7.1.1（相位 + `bPedestrianGreen` / `bExclusivePedestrian`）。

### 7.14 里程碑 M1–M4 字段与交互细化

| 阶段     | 数据字段（最小集）                                                                                                                      | 工具 / 交互                                    | 下游可消费           |
| ------ | ------------------------------------------------------------------------------------------------------------------------------ | ------------------------------------------ | --------------- |
| **M1** | `CrossedRoad, S0/S1, T0/T1, Variant, CrosswalkStyle, bStopLine, StopLineStyle`                                                 | 画横道曲线→吸附 road→生成；详情面板编辑；路口勾选自动生成 4 个       | 仅可视化（斑马线 + 停止线） |
| **M2** | + `bYieldToPedestrian, bSpeedBump, SignalController*(空)`                                                                       | 同上 + 属性面板勾选行为                              | L2 可实现让行/减速     |
| **M3** | + `bSignalized, bActuated, bExclusivePedestrianPhase, bAudibleSignal, bSchoolZone, SignalHeads[]` + `URoadSignalController` 实装 | 信号控制器编辑（相位表）；路口型挂 junction 控制器；独立型自建；信号头放置 | L2 可跑相位调度       |
| **M4** | 接入 P1 车道图：`Crossing` 在行人图加边、在车行 lane 边加 `FCrossingConstraint`                                                                  | 图重建自动纳入                                    | 寻路/AI 用约束       |

> 推荐起步决策（此前已给默认）：① 下游消费选 **a（直接持 UObject）**；② **先做 M1**（行为留接口）；③ 减速带/让行 **先做 Crossing 属性**，字段预留抽 `URoadFurniture` 的余地。

### 7.15 小结：判定清单（实现前逐项核对）

- [ ] 横道 = 独立 `URoadCrossing`，**绝不**进 lane/boundary 段（§7.3.1）
- [ ] 几何只存 `CrossedRoad + [S0,S1] + [t0,t1]`，两侧边界由 `CreatePolyline` 派生（§7.3/7.12.1）
- [ ] 停止线 = 横向标线实体，**不进** `LaneMarking`（§7.12.5）
- [ ] 让行标志 = 路侧点状，与横道正交（§7.12.5）
- [ ] 两类统一，差异仅 `SignalController` 引用（§7.0）
- [ ] 行为字段全在 L0 定义、L2 消费，RoadBuilder 不实现"把车停下"（§7 开头）

### 7.11 待用户拍板

1. 下游消费方/导出格式（a 直接 UObject / b OpenDRIVE / c 自定义 JSON）——决定 M1 字段与是否做导出器。**推荐 a**。
2. 先做 M1（数据+视觉）还是连行为一起。**推荐先做 M1**。
3. 减速带/让行属性化（`URoadCrossing` 字段）还是独立道路家具（`URoadFurniture`）。**推荐先做属性，字段预留抽离余地**。
