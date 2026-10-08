# RoadBuilder 人行横道 / 横向设施（M1）设计 · 权威文档

> **本文档是 clone 稳定的权威版**，会被 `git` 跟踪，换机 `clone` 后可直接读到。
> 本地工作镜像：`.workbuddy/memory/HANDOFF-Crosswalk.md`（被 `.gitignore` 忽略，仅本机速查，含更多代码事实/问题档案）。
> 二者设计结论一致；若冲突以本文件为准。
>
> 状态（2026-10-08）：设计 100% 落盘，**业务代码 0% 未动**。暂停自动提交，需用户实机确认后再提交。
> 最新代码提交：`406883f`（MarkStyles 文件夹拆分，已提交）。

---

## 0. 范围与目标

M1 = 人行横道（Crosswalk）/ 横向设施的设计与数据结构，不含车辆行驶行为（那是独立仿真/AI 层，不在 RoadBuilder）。

- 核实结论：现有 marking 系统**已 road-bound**（`UMarkingCurve`/`UMarkingPoint` : `URoadMarking` : `UObject`，`NewObject<...>(this)` 以 road 为 Outer，存 `ARoadActor::Markings`）。真正缺失的是三层：
  1. **语义类型**（marking 自身零语义字段，种类靠 `MarkStyle`/`Mesh` 资产隐式区分）；
  2. **作用车道引用**（只有 `(S,T)`，无 LaneRef）；
  3. **crossing 查询 API**（无 `GetCrossingsAt`）。
- 车道作用域（LaneSpan/LaneRef）留 M3/M4，M1 不做。

---

## 1. 架构决策：路线 (b) —— 独立 `URoadCrossing`

> 早前 §5.6 曾拍板路线 (a)（扩展现有 marking），**2026-10-08 推翻，改选 (b）**。

- **(b) 新建独立 `URoadCrossing` UObject**，人行横道/停止线/减速带抽离 `UMarkingCurve`/`UMarkingPoint`。
- `URoadCrossing` **几何私有**（存 `(S,T)` 折线，复用 `FMarkingCurvePoint` struct，**不继承** `UMarkingCurve`），**不注册进 `Road->Markings`**，住 `Scene::Crossings`，由 crosswalk 专用通道渲染 —— 从根上消除"引用 UMarkingCurve 会双渲染"的 bug。
- 路线 (a) 的 `FRoadCrossingInfo` 随 (a) 废弃。

### 选项 Z（待拍板，未采用）
若 crossing 实例进 `Road->Markings`、只由通用 marking 通道渲染、`URoadCrossing` 不设独立 renderer：
- 优点：白嫖存储/序列化/渲染/选中/DeleteMarking（marking 渲染全库仅 `RoadActor.cpp:1467-1468` 一处）。
- 代价：`URoadCrossing` 必须继承 `URoadMarking`（或 `UMarkingCurve`）才能入数组；需审计 `RoadActor.cpp:429`、`:1691` 两处遍历 `Markings` 的通用路径加 `Cast<URoadCrossing>` 守卫。
- 当前 §4.2/§4.3 仍定 **组合 + `Scene::Crossings` + `ARoadCrossingActor`**。

---

## 2. 枚举设计（按几何拆成两个集合）

> 决策：**不放在 `URoadMarking` 基类大全集**，而是按几何拆成 `EMarkingCurveType`(曲线) / `EMarkingPointType`(点) 两个枚举，分别落子类。

### 2.1 `EMarkingCurveType`（`UMarkingCurve`；路线 b 当下内容）
- `LaneLine` 车道线（纵向边界，`ULaneMarkStyle`）
- `FillArea` 填充区/导流岛（多边形，`UPolygonMarkStyle` + `bClosedLoop`）
- `Other` 兜底（旧资产默认、未知；`AddMarkingCurve` 按 `MarkStyle` 运行时类型回填）
- *若选项 Z*：此处追加 `Crosswalk`/`StopLine`/`SpeedBump`，`ECrossingType` 作废。

### 2.2 `EMarkingPointType`（`UMarkingPoint`）
- `Arrow` 转弯箭头（配 `EArrowDirection`）
- `YieldMark` 让行标记
- `StopSign` 停止标志牌（点状 mesh；与"停止线"区分见 §2.3 注释）
- `Other` 兜底

### 2.3 `ECrossingType`（`URoadCrossing`；横向带状设施）
- `Crosswalk` 人行横道（配 `ECrosswalkVariant`）
- `StopLine` 停止线（横向粗线）
- `SpeedBump` 减速带（横向凸起）
- 未来可扩 `YieldLine`(让行线) / `BusStop`(公交停靠区)
- ⚠️ `StopSign`(点状牌) vs `StopLine`(横向地面线) 可并存；MVP 默认保留 `StopSign` 占位，待用户确认是否删。

### 2.4 `EMarkingKind` 统一视图 + 基类虚 `GetKind()`（必需）
- 合并两子集 `{ LaneLine, FillArea, Arrow, YieldMark, StopSign, Other }`（不含 crossing）。
- 基类 `virtual EMarkingKind GetKind() const { return Other; }`；子类 override 转回。查询 API `GetMarkingsAt` 用一处过滤，不必先 `Cast` 再 `switch`。

### 2.5 独立子枚举
- `ECrosswalkVariant`：`Zebra`/`Parallel`/`ZebraWithBorder`/`Continental`/`Simulated`（视觉花纹类型，作 `UCrosswalkStyle` 字段，不要每种一个类）。
- `EArrowDirection`：`Straight`/`Left`/`Right`/`UTurn`/`Combine`（现状真缺口：箭头方向只存 `Mesh` 旋转，L2 无法消费）。
- `ELaneRole`（推荐）：`None`/`Bus`/`Bike`/`HOV`/`Taxi`/`NoParking`/未来`Tidal`/`Variable`。挂 `UMarkingCurve` 当 `CurveType==LaneLine` 时，承载专用车道/禁停语义，避免把 `DedicatedLaneLine` 升顶层。

### 2.6 `EMarkingCurveType` 后续扩展候选（参考 CS2 / OpenDRIVE / Lanelet2 / 真实道路）

**核心原则：type 管"角色/几何类"，appearance 管"长相"，二者必须分离。**
错把 `DoubleLaneLine`/`DashedLaneLine`/`YellowLine` 加进枚举是反模式——这些是 roadMark 的**外观属性**（OpenDRIVE/Lanelet2 里 `type=solid/broken` + `color` + `width`）。RoadBuilder 已有 `ELaneMarkType` 承接双黄/白实/虚线，颜色由 Material 名推断。

真实世界绝大多数"新标线"落进已有三桶：
- 纵向线状 → `LaneLine`（加 `ELaneRole` 标志）
- 闭合填充 → `FillArea`（加 pattern：导流/斜纹/接近障碍物）
- 横跨带状 → crossing 家族（`ECrossingType`）

**真正值得升顶层的 Tier A（少数）：**
1. `EdgeLine` / `CurbLine`（道路边缘线/路缘石）——Lanelet2 `road_border`/`curbstone`、OpenDRIVE `edge`/`curb`；阻断变道、可渲染 3D 路缘唇。
2. `DedicatedLaneLine`（公交/非机动车/HOV/出租）——**推荐用 `LaneLine + ELaneRole`**，避免枚举膨胀。
3. `ParkingLine`（车位线，重复短段）——`LaneLine + pattern=Parking` 或独立。
4. `BottsDots` / `RaisedMarker`（突起路标，OpenDRIVE `botts dots`）——`LaneLine + pattern=Dots` 或独立。

**横向类一律走 `ECrossingType`**（与 Crosswalk/StopLine 同构，都是"S 定位 + 横向带状"）：`YieldLine`(让行线) / `RailwayCrossing`(铁路道口白网格) / `SpeedReduction`(横向减速漆线，**≠ SpeedBump 物理凸起**)。

**填充类一律走 `FillArea` + pattern**（不进本枚举）：导流线/路口导向线/斜纹填充/接近障碍物标线（Lanelet2 `keepout`；OpenDRIVE `grass` 边）。

**开源/商用参考：**
- **OpenDRIVE `e_roadMarkType`**：none/solid/broken/solid solid/broken broken/broken solid/solid broken/botts dots/curb/grass/edge/custom + `color` + `laneChange`。⚠️ 它把"外观"与"边界"混在同一 `type`，不照搬。
- **Lanelet2**：`line_thin`(solid/solid_solid/dashed/dashed_solid/solid_dashed)/`line_thick`/`curbstone`(high/low)/`road_border`/`virtual` + symbol(`arrow`/`stop_line`/`zig-zag`/`bump`)+ lanelet subtype(`bus_lane`/`bicycle_lane`/`crosswalk`)。与"type+style+crossing"三段式一致。
- **CS2（天际线2）**：标线按道路类型程序化生成、无颗粒枚举；crosswalk/stopline 是路口可增删设施；中心线颜色可切欧式白/美式黄。印证"车道线应是 road/lane 配置属性而非逐条枚举"。
- **真实道路（GB 5768 / MUTCD）**：纵向(车道线/边缘线/中心双黄) / 横向(停止线/让行线/横道/减速丘/铁路道口/减速标线) / 填充(导流/斜纹) / 字符符号(箭头/文字) / 特殊(突起路标/施工临时)。

**落地建议（最终）**：M1 `EMarkingCurveType` 维持 `LaneLine`/`FillArea`/`Other`（Z 下加 Crosswalk/StopLine/SpeedBump）。Tier A 里优先只补 `EdgeLine`/`CurbLine`；其余先用 `LaneLine + ELaneRole/pattern` 顶着。枚举始终小、每个 `switch` 无死分支。

---

## 3. 数据结构

- **`URoadCrossing : UObject`**（新建 `Source/RoadBuilder/Public/RoadCrossing.h` + `.cpp`）：
  - `ECrossingType Type`
  - `ARoadActor* Road`（归属；或 road 为 Outer，沿用 road-bound 范式）
  - `double S`（纵向位置）/ `double Width`（横向跨度）/ `double Length`（沿路厚度：横道=带宽、停线≈0、减速带≈条宽）
  - `ECrosswalkVariant Variant`（仅 `Crosswalk` 生效）
  - **几何私有**：存横向带状的 `(S,T)` 折线/点（或私有 `URoadCurve`），**不注册进 `Road->Markings`**。
  - 行为占位：`bool bYieldToPedestrian` / `double InfluenceLength` / `bool bSignalized` / `URoadSignalController* SignalController`（前向声明占位）。`bSpeedBump` 冗余于 `Type==SpeedBump`，删除。
- 枚举字段分别加在两子类（不放大全集到基类）：`UMarkingCurve::CurveType`、`UMarkingPoint::PointType`，默认 `Other`，旧资产 `PostLoad` 回填。
- `ARoadScene` 加 `UPROPERTY() TArray<URoadCrossing*> Crossings`（推荐 Scene 集中持有）。

---

## 4. 渲染分派

- 全库仅 `ARoadActor::BuildMesh`(`RoadActor.cpp:1426`) 在 `:1467-1468` 遍历 `Markings` 调 `Marking->BuildMesh` 一处。
- crossing 走专用通道（不进 `Markings`）。M1 渲染三选一：
  1. **（推荐，零新类）**`UMarkingCurve::BuildMesh` 按 `ECrossingType` 语义驱动最小渲染（StopLine→白色横向实心粗带、SpeedBump→凸条/斜纹、Crosswalk 仍走 `UCrosswalkStyle`）。
  2. （增强）每类加 `UStopLineStyle`/`USpeedBumpStyle` 资产。
  3. （重构）泛化 `UCrosswalkStyle`→`UTransverseMarkStyle`（加 Pattern 枚举）。
- 现状 `UBaseMarkStyle` 子类仅 LaneMarkStyle/CrosswalkStyle/PolygonMarkStyle 三个，**无** StopLine/SpeedBump 样式 —— 不能所有 crossing 都用 `UCrosswalkStyle`（会被画成斑马）。

---

## 5. 查询 API（缺失层 ③）

- `ARoadActor::GetMarkingsAt(S/T range, EMarkingKind Filter)` —— 用 `GetKind()` 一处过滤。
- `ARoadScene::GetCrossings(Road)` / `ARoadActor::GetCrossingsAt(S)` —— 返回该路/该处的 `URoadCrossing`。

---

## 6. 任务拆分 D1–D7

- **D1** 加 `EMarkingCurveType`/`EMarkingPointType`（分置 `UMarkingCurve`/`UMarkingPoint`，`RoadMarking.h`）+ `EMarkingKind`（基类虚 `GetKind()`）+ `ECrossingType` + `ECrosswalkVariant` + `EArrowDirection` + `ELaneRole`（`RoadCrossing.h`）。
- **D2** `URoadCrossing` 数据结构（`RoadCrossing.h/.cpp`）。
- **D3** `ARoadScene::Crossings` 容器 + 序列化。
- **D4** `RoadTool_Crossing` + `ARoadCrossingActor`（画线交互复用 marking 曲线工具）。
- **D5** `Scene::GetCrossings` / `Road::GetMarkingsAt` 查询 API。
- **D6** crossing 按 `ECrossingType` 渲染分派（斑马复用 `UCrosswalkStyle`）。
- **D7** 行为字段占位（不实现逻辑）。

---

## 7. clone 连续性（重要）

- **`.workbuddy/` 被 `.gitignore` 第 41 行忽略**，不会随 `git clone` 走。本文件（`Docs/Crosswalk_M1_Design.md`）是随 git 走的权威版。
- 换机前请务必：
  ```bash
  git add Docs/Crosswalk_M1_Design.md
  git commit -m "Docs: add authoritative M1 crosswalk/transverse facility design"
  ```
- 用户级 `MEMORY.md`（`C:\Users\111\.workbuddy\user-...-personal\MEMORY.md`）是本机文件、**不随 git 也不随仓库**；跨机只靠服务器端注入的云 profile（账号级）。项目级设计请以本文件为准。
- 资料库（Library）可作备选：把本 md 作为文档上传到 WorkBuddy 资料库，任意机器登录账号可读；但它是**独立副本**，不与 git 自动同步，且改了要手动再传。优先用 git 跟踪本文件。
