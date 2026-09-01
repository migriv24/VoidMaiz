---
type: Reference
title: The pre-Void-Core concept draft (v1, superseded)
description: The original AI-drafted concept document, archived verbatim with a critique. Superseded by the concepts/ folder on founding day.
tags: [status:superseded, audience:dev, confidence:exploratory]
timestamp: 2026-07-09T00:00:00Z
---

> **Status: SUPERSEDED (2026-07-09, founding day).** This document was drafted by
> an outside AI agent that did not know Void Core. It is archived because it
> captures the author's *intent* well — and because its central "radical" idea
> (log-first, CLI-primary, replayable state, agent-native) is a description of
> **what Void Core already is**, which is itself the founding insight (see
> [log-first inheritance](/concepts/log-first-inheritance.md)).
>
> **Disposition of its sections:**
> - §2 (log-first architecture) → inherited from Void Core, not built. Mapping
>   table in [log-first inheritance](/concepts/log-first-inheritance.md).
> - §3 (AI protocol) → the dispatcher already is the agent surface; transport
>   (WebSocket/gRPC) is a host holiday. Attribution → upstream ask.
> - §4 (graphs as programming language, execution engine, compilation targets)
>   → **cut**: violates the compute boundary; applications compute. Rule storage
>   & reduce belong to Void Core's transform layers.
> - §5 (physical devices as nodes) → reframed as **input holidays** emitting
>   dispatcher commands; later-phase ([roadmap](/roadmap.md)).
> - §6 (git-like collaboration, tiered storage, BaaS) → **cut** from the library:
>   state documents are diffable JSON (use git); branch/merge of histories is
>   upstream research; sync/cloud is a host concern.
> - §7 (pluggable rendering, custom node widgets) → **kept**, the heart of the
>   project: [views as projections](/concepts/views-as-projections.md).
> - §8 (architecture layers) → redrawn without a Log Manager or Graph Engine
>   (both are Void Core).
> - §10 (roadmap) → replaced by the VLS-parity-gated [roadmap](/roadmap.md).
> - §11 (research questions) → the good ones (merge semantics, semantic diff,
>   replay performance, attribution) forwarded upstream or kept in
>   [developer questions](/developer_questions.md); sandboxing = holiday design.
> - The academic references (§4.2, §5.3) are retained below for later reading.
>
> Everything below this line is the draft, verbatim.

---

# Project Concept Document: **Void Node**
*A Next-Generation Node Graph Library Built on the Void Core*

---

## 1. Executive Summary

**Void Node** is a radical reimagining of node-based visual programming, built on the **Void Core** infrastructure. Unlike existing systems (LiteGraph, Blender Nodes, Unreal Blueprints, TouchDesigner), Void Node is **log-first**: the transaction log is the source of truth, and the visual interface is merely a reactive view over that log. This architectural inversion enables unprecedented capabilities:

- **Native AI collaboration** – agents operate on the same log humans do
- **Full version control** – graphs are diffable, mergeable, and forkable like code
- **Pluggable rendering** – the same graph can look like a 2D canvas, 3D space, spreadsheet, or timeline
- **Physical embodiment** – MIDI controllers, sensors, and IoT devices become first-class nodes
- **Operating system semantics** – graphs can represent running processes, pipes, and system state

The library is being built in C++20 atop the **Void Core** infrastructure, with rendering targeting OpenGL/Vulkan.

---

## 2. Core Philosophy: The Log-First Architecture

### 2.1 The Transaction Log as Source of Truth

In Void Node, **the graph state IS the log**. Every operation—adding a node, connecting ports, changing a parameter—is recorded as an immutable, append-only transaction. The in-memory graph is always **replayed from the log** on load.

**Design Principle:** The log is human-readable (CLI commands), machine-replayable (undo/redo/automation), and AI-editable (LLMs can generate transactions).

**Transaction Structure:**
```json
{
  "id": "txn_abc123",
  "timestamp": "2026-07-09T14:32:11Z",
  "user": "agent_7b3f",
  "operation": "connect_ports",
  "params": {
    "from_node": "oscillator_1",
    "from_port": "frequency_out",
    "to_node": "speaker_1",
    "to_port": "audio_in"
  },
  "metadata": {
    "ai_reasoning": "Connect oscillator to speaker for audible output",
    "session_id": "sess_9f2e"
  }
}
```

### 2.2 CLI as Primary Interface

Every user interaction maps to a CLI command. This means:
- **Human operators** can script complex graph manipulations
- **AI agents** can generate and execute commands directly
- **Third-party tools** can integrate by emitting log entries
- **Live coding** becomes native—modify the graph via text commands

**Example CLI Session:**
```bash
> graph new "audio_processor"
> node add sine_oscillator --type audio.oscillator --params '{"frequency":440}'
> node add gain_stage --type audio.gain --params '{"gain_db":-6}'
> node add speaker_output --type audio.output
> connect sine_oscillator:signal -> gain_stage:input
> connect gain_stage:output -> speaker_output:audio
> graph play
```

---

## 3. Universal UI for AI Agents

### 3.1 Bidirectional Communication

Void Node exposes a **bidirectional protocol** (WebSocket/gRPC/REST) for AI agents:
- **Read:** Agents can query graph state, history, and metadata
- **Write:** Agents can submit transactions (with reasoning attached)
- **Stream:** Real-time updates as humans edit the graph

### 3.2 AI as Co-Creator

**Capabilities:**
- **Natural Language → Graph:** "Create a reverb effect chain" → agent generates transaction sequence
- **Graph → Natural Language:** Agent summarizes complex graphs for human understanding
- **Auto-completion:** Suggest next nodes based on context (like code autocomplete)
- **Debugging:** Analyze log for anomalies and propose fixes
- **Optimization:** Restructure graphs for performance

### 3.3 Agent Identity & Attribution

Every transaction includes a `user` field (human or agent ID). This enables:
- **Audit trails:** Who (or what) made each change
- **Collaborative workflows:** Humans and agents working side-by-side
- **Trust calibration:** Users can accept/reject agent-proposed transactions

---

## 4. Node Graphs as Programming Language

### 4.1 Semantic Foundation

Drawing from academic research, Void Node treats node graphs as **first-class programming constructs**, not just visual sugar. This means:

- **Type System:** Every port has a type (int, float, string, audio, video, tensor, etc.)
- **Effects System:** Nodes can have side effects (I/O, network, file system)
- **Compilation Targets:** Subgraphs can be compiled to C++, GLSL, Python, CUDA, or SQL
- **Higher-Order Functions:** Nodes can accept other graphs as parameters (meta-programming)

### 4.2 Academic References

| Paper | Key Insight | Application to Void Node |
|-------|-------------|--------------------------|
| **"A Graph-Based Higher-Order Intermediate Representation"** (CGO 2021) | Graphs can represent control flow, dataflow, and type information simultaneously | Void Node nodes encode type signatures; the execution engine supports both dataflow and control-flow modes |
| **"Dataflow Graphs as a Medium for Programming"** (Arvind, 1980s) | Dataflow naturally expresses parallelism; graphs are executable semantics | The transaction log can be "compiled" to an execution plan; nodes run in parallel when data dependencies allow |
| **"Subtext: Uncovering the Relationships Between Visual Programs"** (Edwards, UIST 2004) | Visual programs have hidden structural relationships; direct manipulation can reveal them | The log enables structural diffing; users can "merge" graphs like code branches, revealing relationships |
| **"Visual Programming Languages and the Empirical Evidence For and Against"** (Whitley, 1997) | Visual languages succeed when they match the problem domain; they fail when overly general | Void Node is **domain-agnostic** but provides strong domain-specific abstraction capabilities through node templates |

### 4.3 Graphs as Executable Code

- **Graph Execution Engine:** Evaluates nodes in topological order, handling data dependencies
- **Conditional Execution:** Support for control-flow nodes (if/else, loops) via special node types
- **Subgraph Nodes:** A node can contain a full graph (hierarchical composition)
- **Function Nodes:** Graphs can be parameterized and invoked like functions

---

## 5. Physical Embodiment & External System Integration

### 5.1 Device Abstraction Layer

Void Node treats physical devices (MIDI controllers, sensors, actuators, cameras, network services) as **first-class nodes** through a device abstraction layer.

**Device Node Lifecycle:**
1. **Discovery:** Device is detected (USB, Bluetooth, network)
2. **Configuration:** User maps device capabilities to node ports (requires CLI/UI setup)
3. **Instantiation:** Device becomes a node in the graph
4. **Operation:** Node streams data in real-time

**Example: MIDI Controller as a Node**
```bash
> device discover --type midi
Found: "Akai Professional MPK Mini" (id: midi_001)
> device configure midi_001 --map "knob_1" -> "frequency" --map "pad_1" -> "trigger"
> node add external.midi_001 --device midi_001
> connect external.midi_001:frequency -> oscillator_1:frequency
> graph play
```

### 5.2 Expressing External Systems as Nested Nodes

Critically, Void Node can **overlap existing systems** and express them as nodes—even nodes within nodes.

**Approach:**
- **System Adapters:** Wrappers for external systems (VST plugins, HTTP APIs, databases, OS processes)
- **Nested Representation:** An external system becomes a node; its internals can be expanded as a subgraph
- **Bidirectional Mapping:** Changes inside the subgraph reflect back to the external system

**Example: Expressing a Docker Container as a Node**
```bash
> node add system.docker_container --image "nginx" --params '{"port":8080}'
> node expand system.docker_container:internal
> # Inside: see nodes for nginx config, volume mounts, environment variables
```

### 5.3 Academic References for Physical Embodiment

| Paper | Key Insight | Application to Void Node |
|-------|-------------|--------------------------|
| **"Physicalization of Data Flow Graphs"** (TEI 2022) | Physical objects can be literal nodes in a dataflow system; tangibility aids understanding | MIDI controllers, sensors, and actuators appear as nodes with physical feedback (LEDs, haptics) |
| **"Tangible Nodes for Live Coding"** (NIME 2021) | Physical controls mapped to visual programming parameters increase expressivity | Device nodes capture knob/slider movements as transactions; can be replayed later |

---

## 6. Git-Like Collaboration & Data Storage Architecture

### 6.1 Version Control Capabilities

Void Node natively supports **distributed version control** inspired by Git:

- **Commits:** A snapshot of the transaction log at a point in time
- **Branches:** Multiple parallel development streams
- **Merges:** Combine changes from different branches (with conflict resolution)
- **Diffs:** Visual and CLI-based comparison of graph versions
- **Rebasing:** Replay transactions from one branch onto another

**CLI Example:**
```bash
> graph branch feature/audio_effects
> graph checkout main
> graph merge feature/audio_effects --resolve-conflicts
```

### 6.2 Data Storage Hierarchy

Void Node employs a **tiered storage architecture**:

| Tier | Content | Format | Access Pattern | Performance |
|------|---------|--------|----------------|-------------|
| **Memory** | Active graph (replayed from log), hot cache | In-memory C++ objects | Sub-millisecond | Fastest |
| **Local Disk** | Transaction log, snapshots, cache, user preferences | JSONL (JSON Lines) / binary protobuf / SQLite | Millisecond | Fast |
| **Cloud (BaaS)** | Shared graphs, branches, metadata, attachments, user accounts | BaaS (Firebase/Supabase/self-hosted) | 100-500ms | Slower but globally accessible |

### 6.3 What Is Stored Where?

#### Memory (RAM):
- **Active graph instance:** The current working graph (replayed from log)
- **Render cache:** Precomputed layouts, textures, shaders
- **Execution cache:** Intermediate node outputs (for speed)

#### Local Disk:
- **Transaction Log (`graph.log`):** Append-only JSONL of all operations ever performed
- **Snapshots (`snapshots/`):** Periodic full dumps of graph state for faster loading
- **Asset Cache (`assets/`):** Node icons, textures, sound samples, etc.
- **User Preferences (`prefs.json`):** UI layout, color schemes, default settings
- **Device Configurations (`devices/`):** Mappings for physical devices
- **Index (`index.db`):** SQLite database for fast queries on large graphs

#### Cloud (Backend-as-a-Service):
- **Graphs:** Shared graph repositories (like GitHub for node graphs)
- **Branches & Commits:** Version history synchronized across users
- **User Profiles:** Accounts, permissions, API keys
- **Device Metadata:** Shared device configuration templates
- **Attachments:** Large assets (sample packs, 3D models, documentation)

### 6.4 Cache vs. Persistent Storage

| Data Type | Cached? | Cache Duration | Persistent Location |
|-----------|---------|----------------|---------------------|
| Transaction Log | No (streamed) | N/A | Disk + Cloud |
| Graph State | Yes (full replay) | Session lifetime | Disk snapshots |
| Node Outputs | Yes (computed) | Until inputs change | Never (recomputed) |
| Layout Positions | Yes | Session + disk on save | Disk (per graph) |
| Assets | Yes (hot) | Configurable (e.g., 1GB LRU) | Disk + Cloud |
| Search Index | Yes (rebuilt on change) | Persistent | Disk (SQLite) |
| User Preferences | Yes | Persistent | Disk + Cloud |

### 6.5 Cloud BaaS Integration

Void Node supports multiple backends through an abstraction layer:

- **Firebase / Supabase:** For real-time collaboration (WebSocket sync)
- **Self-hosted Node:** Custom server for enterprise deployments
- **IPFS / Filecoin:** Decentralized graph storage (future)

**Sync Strategy:**
1. Local changes are appended to the local transaction log
2. On connectivity, log is **replayed** to the cloud (idempotent)
3. Cloud merges logs from multiple users (with conflict resolution)
4. All clients receive real-time updates via WebSocket

---

## 7. Pluggable Rendering & Visual Metamorphosis

### 7.1 The Grammar of Graphics & Visualization Meta-Languages

In academic HCI, this is called **"grammar of graphics"** or **"visualization meta-languages."**

**Core Principle:** The underlying data model (nodes, edges, ports, properties) is **fixed**, but the **rendering is fully pluggable**. This means Void Node can render the same graph in radically different visual forms:

| Rendering Mode | Visual Style | Use Case |
|----------------|--------------|----------|
| **Classic 2D** | Boxes with ports, bezier curves | Standard editing |
| **3D Spatial** | Floating spheres with glowing beams | Complex graph exploration |
| **Timeline** | Nodes as events, edges as dependencies | Temporal sequencing |
| **Spreadsheet** | Rows = nodes, columns = properties | Data manipulation |
| **Code View** | Textual representation (like OpenSCAD) | Programmers |
| **Physical Mimicry** | Virtual instruments with knobs | Music production |
| **Network Graph** | Force-directed layout | Social/network analysis |
| **Tree View** | Collapsible hierarchy | System architecture |

### 7.2 Custom Node Widgets

Each node can provide its own **renderer** (not just a color, but a full UI component):
- A **filter node** renders as a frequency response curve
- An **oscillator** renders as a waveform
- A **webcam** renders as live video inside the node
- A **MIDI controller** renders as a virtual knob panel
- A **Docker container** renders as a terminal emulator

### 7.3 Implementation Approach

```cpp
// Renderer interface
class GraphRenderer {
public:
  virtual void render(Node& node, RenderContext& ctx) = 0;
  virtual void renderEdge(Edge& edge, RenderContext& ctx) = 0;
  virtual InputResponse handleClick(Point click) = 0;
  // ...
};

// Node provides its own renderer
class CustomNode : public Node {
  std::unique_ptr<NodeRenderer> renderer;
};
```

---

## 8. Architecture Overview

### 8.1 System Layers

```
┌─────────────────────────────────────────────┐
│         Application Layer                   │
│  (Audio DAW, Game Engine, Data Studio, etc.)│
├─────────────────────────────────────────────┤
│         Void Node API (C++20)               │
├─────────────────────────────────────────────┤
│  Graph    │  Log      │  Render  │  Device  │
│  Engine   │  Manager  │  Engine  │  Manager │
├─────────────────────────────────────────────┤
│            Void Core Infrastructure         │
├─────────────────────────────────────────────┤
│  OpenGL/  │  File     │  Network │  BaaS   │
│  Vulkan   │  System   │  Stack   │  Client │
└─────────────────────────────────────────────┘
```

### 8.2 Key Components

| Component | Responsibility |
|-----------|---------------|
| **Log Manager** | Append transactions, replay logs, manage checkpoints |
| **Graph Engine** | Maintain graph state, execute dataflow, handle dependencies |
| **Renderer** | Pluggable rendering of graph in any visual form |
| **Device Manager** | Discover, configure, and bridge physical devices |
| **Storage Layer** | Manage local disk, cache, and cloud synchronization |
| **CLI Interface** | Parse human-readable commands, generate log entries |
| **AI Protocol** | Expose bidirectional API for AI agents |

---

## 9. Use Cases & Applications

Void Node is designed to be **domain-agnostic** and applicable to:

| Domain | Application |
|--------|-------------|
| **Audio/Music** | DAW-like patching, live coding, sound design |
| **Game Development** | Visual scripting, behavior trees, AI logic |
| **Data Science** | ETL pipelines, ML model orchestration |
| **IoT/Physical Computing** | Smart home automation, robotics, interactive art |
| **System Administration** | Infrastructure-as-code, container orchestration |
| **Generative Design** | Procedural content generation, parametric modeling |
| **Scientific Computing** | Simulation pipelines, data visualization |
| **Operating Systems** | Process management, inter-process communication |

---

## 10. Development Roadmap

| Phase | Duration | Milestones |
|-------|----------|------------|
| **0: Foundation** | 3 months | Void Core integration, log data structure, CLI parser |
| **1: Graph Engine** | 2 months | Node/edge models, execution engine, basic transactions |
| **2: Renderer** | 3 months | OpenGL renderer, 2D view, pluggable renderer interface |
| **3: Log Storage** | 2 months | Local disk storage, snapshots, replay, undo/redo |
| **4: AI Protocol** | 2 months | WebSocket/gRPC API, transaction streaming, agent SDK |
| **5: Device Layer** | 3 months | MIDI/OSC/HTTP device abstraction, configuration UI |
| **6: Collaboration** | 3 months | Git-like branching, merging, BaaS sync, cloud storage |
| **7: Compilation** | 3 months | Code generation backends (C++, GLSL, Python, CUDA) |
| **8: Advanced Views** | 3 months | 3D rendering, timeline view, custom node widgets |
| **9: Ecosystem** | Ongoing | Plugin SDK, community templates, documentation |

---

## 11. Open Research Questions

1. **Conflict Resolution:** How to merge diverging transaction logs from multiple users/AI agents?
2. **Semantic Diffing:** What does a "meaningful" diff look like between two graphs?
3. **AI Trust:** How to annotate AI-generated transactions so humans can verify them?
4. **Performance:** How to replay large logs (millions of transactions) efficiently?
5. **Device Synchronization:** How to handle devices that appear/disappear dynamically?
6. **Security:** How to sandbox node execution (especially for external systems)?

---

## 12. Conclusion

Void Node represents a **paradigm shift** in node-based programming. By making the **transaction log** the foundation, it enables AI collaboration, distributed version control, physical embodiment, and limitless visual representation—all while remaining domain-agnostic and extensible.

Built on the Void Core and written in C++20, Void Node will be the **foundation** for next-generation creative tools, AI-assisted development environments, and cyber-physical systems. It is not just a library—it is a **medium** for human-AI collaboration and computational expression.

---

**Document Version:** 1.0
**Last Updated:** 2026-07-09
**Project Codename:** Void Node
**Status:** Concept Phase
