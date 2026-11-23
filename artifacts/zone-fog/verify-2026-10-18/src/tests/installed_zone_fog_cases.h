// Included inside the material test namespace; uses its native GPU/readback helpers.
void TestInstalledZoneFog(IDirect3DDevice9* device, const std::filesystem::path& root,
                          const std::filesystem::path& output)
{
    struct Case { const char *name, *dat, *weather; float x,y,z,dx,dz; };
    // Native DAT coordinates, near known arrivals, looking into each zone.
    const Case cases[] = {
        {"pashhow", "ROM/0/125.DAT", "f_or", -543,18,-630, 0,1},
        {"jugner", "ROM/0/114.DAT", "f_ju", -570,-14,-441, 1,0},
        {"konschtat", "ROM/0/90.DAT", "f_ko", -307,-20,233, .24f,.97f},
        {"la-theine", "ROM/0/115.DAT", "f_la", 775,22,-40, -1,0},
        {"valkurm", "ROM/0/102.DAT", "f_ki", 60,-10,-130, 0,1},
        {"zitah", "ROM2/0/2.DAT", "f_li", -460,-4,-465, 0,1},
    };
    std::ofstream metrics(output / "metrics.csv");
    metrics << "zone,minute,weather,near,far,fog_color,changed_pixels,closer_to_fog\n";
    for (const auto& c : cases)
    {
        const auto path = root/c.dat;
        std::ifstream stream(path, std::ios::binary|std::ios::ate);
        Check(stream.good(), "open installed fog zone");
        if (!stream.good()) continue;
        std::vector<BYTE> bytes(static_cast<size_t>(stream.tellg()));
        stream.seekg(0); stream.read(reinterpret_cast<char*>(bytes.data()), bytes.size());
        noeRAPI_t rapi(device); rapi.SetCurrentFilePath(path.string().c_str());
        ff11Opts_t options = {}; options.renderWater = true; options.renderEnvironment = true;
        const auto previous = gpFF11Opts; gpFF11Opts = &options;
        int count = 0;
        auto* model = Model_FF11_LoadDAT(bytes.data(), static_cast<int>(bytes.size()), count, &rapi);
        gpFF11Opts = previous;
        Check(model != nullptr, "load installed fog zone geometry");
        if (!model) continue;
        ZoneModelRenderMetadata::Prepare(model,device);
        const auto identity = D3DMath::BuildIdentity();
        const auto view = D3DMath::BuildLookAtLH(c.x,c.y,c.z,c.x+c.dx*100,c.y+2,c.z+c.dz*100);
        const auto projection = D3DMath::BuildPerspectiveFovLH(.9f,1,.1f,2500);
        for (int minute : {0,720}) for (const char* weather : {"fine", "mist"})
        {
            const auto weatherPath = std::string(c.weather)+"/weat/"+weather;
            // Fresh state avoids capturing a weather transition before it settles.
            std::vector<ff11EnvironmentRecord_t> records;
            for (const auto& record : gFF11LastEnvironmentRecords)
                if (record.directoryPath == weatherPath) records.push_back(record);
            Check(!records.empty(), "requested real zone weather exists");
            if (records.empty()) continue;
            ZoneEnvironmentState::Data env; ZoneEnvironmentState::Cache cache; int weatherIndex = 0;
            ZoneEnvironmentState::Update(env,cache,weatherIndex,records,minute);
            Check(env.valid && env.fogFar > env.fogNear, "real zone has active authored fog");
            ModelRenderer::Context context;
            context.device = device; context.rendersZoneObjects = true; context.lightingQuality = 1;
            context.minuteOfDay = static_cast<float>(minute); context.useAuthoredLightColor = true;
            context.authoredMainLightColor = env.terrainMainLightColor;
            context.authoredSecondaryLightColor = env.terrainSecondaryLightColor;
            context.authoredAmbientLightColor = env.terrainAmbientColor;
            context.authoredLightPower = env.terrainLightPower;
            context.authoredFogColor = env.fogColor;
            context.authoredFogNear = env.fogNear; context.authoredFogFar = env.fogFar;
            context.cameraPosition[0]=c.x; context.cameraPosition[1]=c.y; context.cameraPosition[2]=c.z;
            context.visibility.lodViewerPoint = context.cameraPosition;
            const auto prefix = std::string(c.name)+"-"+weather+"-"+std::to_string(minute);
            const auto draw = [&](bool fog)
            {
                device->SetTransform(D3DTS_VIEW,&view);
                device->SetTransform(D3DTS_PROJECTION,&projection);
                device->Clear(0,nullptr,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,env.clearColor,1,0);
                ZoneSkyDome::Parameters sky;
                sky.environmentValid=env.valid; sky.indoor=env.indoor;
                sky.radius=env.skyRadius; sky.spokeCount=env.sphereSpokeCount;
                sky.ringCount=env.ringCount; sky.ringColors=env.ringColors; sky.ringElevations=env.ringElevations;
                ZoneSkyDome::Draw(device,sky,c.x,c.y,c.z,2500);
                context.useAuthoredFog=fog;
                for (const auto pass : {ModelRenderer::GeometryPass::Opaque, ModelRenderer::GeometryPass::Transparent})
                {
                    context.geometryPass=pass;
                    ModelRenderer::PrepareFixedFunctionPass(context,identity);
                    ModelRenderer::DrawGeometry(context,model,identity);
                }
            };
            const auto fogged = Frame(device,[&]{draw(true);},output/(prefix+".bmp"));
            const auto clear = Frame(device,[&]{draw(false);},output/(prefix+"-disabled.bmp"));
            size_t changed=0, closer=0;
            const auto distance = [&](DWORD pixel)
            {
                int sum=0;
                for(int shift : {0,8,16}) sum+=std::abs(int((pixel>>shift)&255)-int((env.fogColor>>shift)&255));
                return sum;
            };
            Check(fogged.size()==640*640 && clear.size()==fogged.size(), "capture real zone at 640 pixels");
            for(size_t i=0;i<(std::min)(fogged.size(),clear.size());++i)
                if(std::abs(distance(clear[i])-distance(fogged[i]))>6)
                { ++changed; if(distance(fogged[i])<distance(clear[i])) ++closer; }
            std::cout<<prefix<<": far="<<env.fogFar<<" changed="<<changed<<" closer="<<closer<<'\n';
            metrics<<c.name<<','<<minute<<','<<weather<<','<<env.fogNear<<','<<env.fogFar<<','
                <<env.fogColor<<','<<changed<<','<<closer<<std::endl;
            Check(changed>1000, "authored fog visibly changes installed zone terrain");
            Check(closer*100>=changed*95, "fog moves zone colors toward authored fog color");
        }
        model->ReleaseD3DBuffers(); // rapi owns and deletes its models.
    }
}
