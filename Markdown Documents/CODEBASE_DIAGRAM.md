# DATura codebase map

Based on the working tree inspected on October 8, 2026. Boxes group related modules; arrows show coordination or data flow, rather than an exhaustive include graph.

```mermaid
flowchart TB
    user["User input · menus · settings"]
    app["Application coordinator — main.cpp<br/>Win32 message loop · scene switching · frame orchestration"]
    user --> app

    subgraph assets["1 · Resolve and load assets"]
        files["FFXI installation DATs<br/>Optional replacement DAT folder"]
        resolve["Paths and source selection<br/>ffxi_paths · ffxi_resource · ffxi_dat_resolver"]
        load["Scene and character loaders<br/>scene_model_loader · player_model_loader<br/>creation_model_loader · zone_room_loader"]
        parse["DAT parsing and model construction<br/>model_ff11 · noesis_rapi<br/>d3d_model_buffers · ffxi_model_lifetime"]
        files --> resolve --> load --> parse
    end
    app -->|load requests| resolve

    subgraph runtime["2 · Update scene and interaction"]
        scene["Loaded models and scene state<br/>Geometry · textures · skeletons · placements"]
        world["Zone systems<br/>Environment · collision · doors · elevators<br/>Object visibility · transitions"]
        actors["Player and character systems<br/>player_controller · orbit_camera<br/>Creation · roster · inventory · stats"]
        npc["NPC interaction and dialogue<br/>npc_conversation_controller · npc_event_runtime<br/>ffxi_event_table · ffxi_event_messages"]
        scene --> world
        scene --> actors
        scene --> npc
    end
    parse --> scene
    app -->|input and updates| world
    app -->|input and updates| actors
    app -->|interactions| npc

    subgraph output["3 · Present the scene"]
        render["Scene drawing<br/>model_renderer · sky · weather · water<br/>NPC nameplates · world items · effects"]
        ui["Game UI and native panels<br/>d3d_ui_renderer · ffxi_main_menu<br/>Character screens · chat · inventory"]
        device["Graphics lifecycle and presentation<br/>renderer_backend → d3d9_device<br/>Direct3D 9"]
        audio["Audio playback<br/>bgw_player · audio_player"]
        render --> device
        ui --> device
    end
    scene -->|models and textures| render
    world -->|visibility and environment| render
    actors -->|pose and camera| render
    npc -->|messages and choices| ui
    app -->|frame orchestration| render
    app --> ui
    app --> audio

    tools["Inspection and editing tools<br/>DAT hex reader · resource/texture browsers<br/>Zone object panel · geometry diagnostics"]
    app --> tools
    scene -->|inspection snapshots| tools
    resolve -->|source diagnostics| tools

    support["Repository support<br/>tests/ — regression executables and visual checks<br/>tools/ — extraction and analysis scripts<br/>Markdown Documents/ — findings and development plans<br/>docker/ — build documentation"]
    support -.->|validation and research| assets
    support -.->|validation| runtime
    support -.->|render checks| output

    classDef coordinator fill:#dbeafe,stroke:#2563eb,color:#172554
    classDef data fill:#dcfce7,stroke:#16a34a,color:#14532d
    classDef peripheral fill:#f1f5f9,stroke:#64748b,color:#0f172a
    class app coordinator
    class files,scene data
    class tools,support peripheral
```

The central architectural fact is that `main.cpp` coordinates many extracted modules and still owns substantial application behavior. The three groups above are conceptual responsibilities within the application, not separate services.

Start with `main.cpp` for control flow, `scene_model_loader.cpp` and `model_ff11.cpp` for asset loading, and `model_renderer.cpp` for drawing. `renderer_backend.h` exposes a graphics lifecycle wrapper, but the current model renderer and loaders use the Direct3D 9 device directly.

The DAT resolver selects physical replacement files while retaining retail logical paths. The hex reader operates on an editable buffer and saves new copies. Research checkouts under `tmp/` and the LandSandBoat source/data trees are supporting material; this diagram does not imply that DATura runs a networked game server.
