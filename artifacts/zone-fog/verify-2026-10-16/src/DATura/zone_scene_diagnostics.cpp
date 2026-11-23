#include "stdafx.h"
#include "zone_scene_diagnostics.h"
#include "noesis_rapi.h"
#include "zone_collision_geometry.h"
#include "zone_environment_identity.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <locale>
#include <sstream>
#include <stdexcept>

namespace ZoneSceneDiagnostics
{
Snapshot Capture(const noesisModel_t& model, const ZoneCollision::Mesh& collision,
    const std::vector<std::string>& hidden,
    const std::map<std::string, ZoneObjectTransform::DebugTransform>& overrides,
    const std::string& source, bool mirrorX, const std::array<float, 3>& nativeViewer)
{
    Snapshot result;
    result.source = source; result.mirrorX = mirrorX; result.nativeViewer = nativeViewer;
    const auto append = [&](const ZoneCoverage::Triangle& triangle)
    {
        if (result.triangles.size() >= ZoneCoverage::Options{}.maxTriangles)
            throw std::runtime_error("Scene exceeds the diagnostic triangle budget; no partial report was produced.");
        result.triangles.push_back(triangle);
    };
    for (const auto& sm : model.submeshes)
    {
        if (ZoneEnvironmentIdentity::IsEnvironmentObjectName(sm.objectName))
        { ++result.environmentMeshes; continue; }
        if (!sm.objectName.empty() && std::find(hidden.begin(), hidden.end(), sm.objectName) != hidden.end())
        { ++result.hiddenMeshes; continue; }
        if (!ZoneLod::Visible(sm.zoneLod, nativeViewer.data()))
        { ++result.lodMeshes; continue; }
        ++result.includedMeshes;
        const auto* material = model.pMatData && !sm.materialName.empty()
            ? model.pMatData->FindMaterial(sm.materialName.c_str()) : nullptr;
        if (!material && !sm.materialName.empty()) ++result.unresolvedMaterials;
        if (!material || material->texIdx < 0) ++result.untexturedMeshes;
        else if (!model.pMatData->textures ||
            material->texIdx >= model.pMatData->texCount || !model.pMatData->textures[material->texIdx])
            ++result.unresolvedTextures;
        if (sm.water) ++result.waterMeshes;
        else if (material && !material->noDefaultBlend) ++result.transparentMeshes;
        else if (material && material->alphaTest > 0) ++result.cutoutMeshes;
        else ++result.opaqueMeshes;
        if (sm.cpuVerts.empty() || sm.cpuIndices.empty())
        { ++result.missingCpuMeshes; continue; }
        const auto found = sm.objectName.empty() ? overrides.end() : overrides.find(sm.objectName);
        const bool transform = found != overrides.end();
        D3DMATRIX matrix = {};
        if (transform) { matrix = ZoneObjectTransform::BuildWorldMatrix(found->second); ++result.transformedMeshes; }
        result.trailingIndices += sm.cpuIndices.size() % 3;
        for (size_t i = 0; i + 2 < sm.cpuIndices.size(); i += 3)
        {
            if (sm.cpuIndices[i] >= sm.cpuVerts.size() || sm.cpuIndices[i+1] >= sm.cpuVerts.size() ||
                sm.cpuIndices[i+2] >= sm.cpuVerts.size()) { ++result.invalidIndexTriangles; continue; }
            ZoneCoverage::Triangle triangle;
            triangle.layer = sm.water ? ZoneCoverage::Layer::Water : ZoneCoverage::Layer::Render;
            for (int corner = 0; corner < 3; ++corner)
            {
                const auto* p = sm.cpuVerts[sm.cpuIndices[i+corner]].pos;
                triangle.points[corner] = transform ? ZoneCoverage::Point{
                    p[0]*matrix._11+p[1]*matrix._21+p[2]*matrix._31+matrix._41,
                    p[0]*matrix._12+p[1]*matrix._22+p[2]*matrix._32+matrix._42,
                    p[0]*matrix._13+p[1]*matrix._23+p[2]*matrix._33+matrix._43}
                    : ZoneCoverage::Point{p[0], p[1], p[2]};
            }
            append(triangle);
        }
    }
    for (const auto& src : collision.Triangles())
    {
        ZoneCoverage::Triangle triangle;
        triangle.layer = ZoneCoverage::Layer::Collision;
        for (int i = 0; i < 3; ++i) triangle.points[i] = {src.p[i][0], src.p[i][1], src.p[i][2]};
        append(triangle);
    }
    return result;
}

std::string Summary(const Snapshot& s, const ZoneCoverage::Options& o, const ZoneCoverage::Result& r)
{
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << "Source: " << s.source << "\r\nFrame: native Y down / Z unchanged / X "
        << (s.mirrorX ? "reflected" : "native") << ". Native LOD viewer: "
        << s.nativeViewer[0] << ", " << s.nativeViewer[1] << ", " << s.nativeViewer[2]
        << "\r\nSnapshot: loaded zone and rooms; current LOD, hidden objects and runtime transforms. "
           "No camera frustum/PVS, actors, sky or weather particles; vegetation is static.\r\n"
        << s.triangles.size() << " triangles; " << s.includedMeshes << " meshes; skipped sky/hidden/LOD: "
        << s.environmentMeshes << '/' << s.hiddenMeshes << '/' << s.lodMeshes
        << ". Transformed meshes: " << s.transformedMeshes
        << "\r\nMaterials (mesh counts): opaque " << s.opaqueMeshes << ", cutout " << s.cutoutMeshes
        << ", transparent " << s.transparentMeshes << ", water " << s.waterMeshes
        << ". Untextured: " << s.untexturedMeshes
        << ". Unresolved named material/texture references: " << s.unresolvedMaterials << '/' << s.unresolvedTextures
        << "\r\nMissing CPU geometry: " << s.missingCpuMeshes << "; invalid index triangles: "
        << s.invalidIndexTriangles << "; trailing indices: " << s.trailingIndices << "\r\n";
    if (!r.success) { out << "Analysis failed: " << r.error; return out.str(); }
    size_t both = 0, renderOnly = 0, collisionOnly = 0;
    for (auto cell : r.cells)
    { both += (cell & 3) == 3; renderOnly += (cell & 3) == 1; collisionOnly += (cell & 3) == 2; }
    out << "Grid: " << r.width << " x " << r.height << "; cell size " << r.cellSize;
    if (r.coarsened) out << " (coarsened from " << o.cellSize << " to stay within budget)";
    out << ". Minimum |normal Y|: " << o.minAbsNormalY << "; Y range: ";
    if (o.useHeightRange) out << '[' << o.minY << ", " << o.maxY << ']'; else out << "all heights";
    out << "\r\nCells: render+collision " << both << "; render only " << renderOnly
        << "; collision only " << collisionOnly << "; water " << r.water.coveredCells
        << "\r\nAccepted render/collision/water triangles: " << r.render.accepted << '/'
        << r.collision.accepted << '/' << r.water.accepted
        << ". Rejected invalid/degenerate/steep/outside Y: " << r.invalidTriangles << '/'
        << r.degenerateTriangles << '/' << r.nearVerticalTriangles << '/' << r.outsideHeightRangeTriangles
        << "\r\nCoverage includes triangle interiors and edge contacts. This is projected occupancy, "
           "not a walkability test or a pass/fail score. Ceilings, overhangs and water naturally differ "
           "from collision; stacked floors overlap. Use a Y slice to inspect interiors.";
    return out.str();
}

namespace
{
std::string Escape(const std::string& value)
{
    std::string escaped;
    for (char c : value)
        switch (c) { case '&': escaped += "&amp;"; break; case '<': escaped += "&lt;"; break;
        case '>': escaped += "&gt;"; break; case '"': escaped += "&quot;"; break;
        default: escaped += c; }
    return escaped;
}
void RequireValid(const ZoneCoverage::Result& r)
{
    if (!r.success || r.width > 16777216 || r.height > 16777216 ||
        r.width * r.height != r.cells.size() || !std::isfinite(r.cellSize) ||
        !std::isfinite(r.minX) || !std::isfinite(r.minZ) ||
        std::any_of(r.cells.begin(), r.cells.end(), [](auto c) { return c > 7; }))
        throw std::runtime_error("Cannot export an incomplete coverage result.");
}
}

void WriteCsv(std::ostream& out, const ZoneCoverage::Result& r)
{
    RequireValid(r);
    out.imbue(std::locale::classic());
    out << std::setprecision(17) << "scene_min_x,scene_min_z,scene_max_x,scene_max_z,render,collision,water\n";
    for (size_t z = 0; z < r.height; ++z) for (size_t x = 0; x < r.width; ++x)
    {
        const auto bits = r.cells[z*r.width+x];
        if (!bits) continue;
        out << r.minX+x*r.cellSize << ',' << r.minZ+z*r.cellSize << ','
            << r.minX+(x+1)*r.cellSize << ',' << r.minZ+(z+1)*r.cellSize << ','
            << ((bits&1)!=0) << ',' << ((bits&2)!=0) << ',' << ((bits&4)!=0) << '\n';
    }
    if (!out) throw std::runtime_error("Could not finish writing the CSV file.");
}

void WriteHtml(std::ostream& out, const Snapshot& s, const ZoneCoverage::Options& o, const ZoneCoverage::Result& r)
{
    RequireValid(r);
    out.imbue(std::locale::classic());
    // Source paths are existing Windows narrow paths; encode explicitly as UTF-8.
    std::string summary = Summary(s, o, r);
    const int wideSize = MultiByteToWideChar(CP_ACP, 0, summary.data(), static_cast<int>(summary.size()), nullptr, 0);
    std::wstring wide(wideSize, L'\0');
    MultiByteToWideChar(CP_ACP, 0, summary.data(), static_cast<int>(summary.size()), wide.data(), wideSize);
    const int utfSize = WideCharToMultiByte(CP_UTF8, 0, wide.data(), wideSize, nullptr, 0, nullptr, nullptr);
    summary.resize(utfSize);
    WideCharToMultiByte(CP_UTF8, 0, wide.data(), wideSize, summary.data(), utfSize, nullptr, nullptr);
    out << R"html(<!doctype html><html lang="en"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>DATura zone geometry diagnostics</title><style>
body{margin:24px;background:#101722;color:#e4ebf5;font:15px system-ui}h1{font-size:26px;margin-bottom:6px}
p{color:#b7c6d9}pre{white-space:pre-wrap;line-height:1.55;font:13px ui-monospace,monospace;background:#192333;padding:16px;border-radius:8px}
label,button{margin-right:18px}button{background:#31435e;color:white;border:1px solid #6681a6;border-radius:5px;padding:8px 14px;cursor:pointer}
canvas{display:block;width:100%;height:60vh;min-height:280px;background:#101722;border:1px solid #43546c;margin-top:16px;touch-action:none}
#hover{min-height:1.5em;font-family:monospace}.legend{display:flex;gap:20px;flex-wrap:wrap;margin:15px 0}.legend span:before{content:' ';display:inline-block;width:12px;height:12px;margin-right:7px;background:var(--c)}
</style><h1>Zone geometry diagnostics</h1><p>Drag to pan, scroll to zoom. +X points right; +Z points down. Smaller Y is higher.</p>
<label><input id="render" type="checkbox" checked>Render</label><label><input id="collision" type="checkbox" checked>Collision</label>
<label><input id="water" type="checkbox" checked>Water</label><button id="reset">Fit map</button>
<div class="legend"><span style="--c:#e4ac58">Render only</span><span style="--c:#e26c80">Collision only</span><span style="--c:#66c59d">Render + collision</span><span style="--c:#548ee8">Water (overlay)</span></div>
<canvas id="map" aria-label="Projected geometry coverage map"></canvas><p id="hover">Move over a cell to inspect its coordinates and original layers.</p><pre>)html"
        << Escape(summary) << "</pre><script>\nconst W=" << r.width << ",H=" << r.height
        << std::setprecision(17) << ",minX=" << r.minX << ",minZ=" << r.minZ << ",cellSize=" << r.cellSize
        << ";\nconst cells='";
    for (auto c : r.cells) out << static_cast<char>('0'+c);
    out << R"html(';
const canvas=document.querySelector('#map'),ctx=canvas.getContext('2d'),tile=document.createElement('canvas'),tc=tile.getContext('2d');
tile.width=Math.max(1,W);tile.height=Math.max(1,H);let scale=1,ox=0,oy=0,drag=null;
function redraw(){ctx.clearRect(0,0,canvas.width,canvas.height);ctx.imageSmoothingEnabled=false;if(W&&H)ctx.drawImage(tile,ox,oy,W*scale,H*scale);else{ctx.fillStyle='#e4ebf5';ctx.font='18px system-ui';ctx.fillText('No geometry retained by these filters.',24,40);}}
function fit(){scale=Math.min(canvas.width/Math.max(1,W),canvas.height/Math.max(1,H))*.94;ox=(canvas.width-W*scale)/2;oy=(canvas.height-H*scale)/2;redraw();}
function paint(){if(!W||!H){redraw();return;}const mask=(document.querySelector('#render').checked?1:0)|(document.querySelector('#collision').checked?2:0)|(document.querySelector('#water').checked?4:0);
const img=tc.createImageData(W,H),colors=[[16,23,34],[228,172,88],[226,108,128],[102,197,157]];
for(let i=0;i<cells.length;i++){const b=Number(cells[i])&mask;let c=colors[b&3];if(b&4)c=(b&3)?c.map((v,j)=>Math.round((v+[84,142,232][j])/2)):[84,142,232];img.data.set([...c,255],i*4);}tc.putImageData(img,0,0);redraw();}
function point(e){const r=canvas.getBoundingClientRect();return [(e.clientX-r.left)*canvas.width/r.width,(e.clientY-r.top)*canvas.height/r.height];}
canvas.addEventListener('wheel',e=>{e.preventDefault();const [x,y]=point(e),next=Math.max(.05,Math.min(1000,scale*Math.exp(-e.deltaY*.001)));ox=x-(x-ox)*next/scale;oy=y-(y-oy)*next/scale;scale=next;redraw();},{passive:false});
canvas.addEventListener('pointerdown',e=>{drag=point(e);canvas.setPointerCapture(e.pointerId);});
canvas.addEventListener('pointerup',()=>drag=null);canvas.addEventListener('pointercancel',()=>drag=null);
canvas.addEventListener('pointermove',e=>{const p=point(e);if(drag){ox+=p[0]-drag[0];oy+=p[1]-drag[1];drag=p;redraw();}const x=Math.floor((p[0]-ox)/scale),z=Math.floor((p[1]-oy)/scale);const target=document.querySelector('#hover');
if(x<0||z<0||x>=W||z>=H){target.textContent='Outside grid';return;}const b=Number(cells[z*W+x]);target.textContent=`X [${(minX+x*cellSize).toFixed(2)}, ${(minX+(x+1)*cellSize).toFixed(2)}]  Z [${(minZ+z*cellSize).toFixed(2)}, ${(minZ+(z+1)*cellSize).toFixed(2)}]  Render ${!!(b&1)} | Collision ${!!(b&2)} | Water ${!!(b&4)}`;});
document.querySelectorAll('input').forEach(e=>e.addEventListener('change',paint));document.querySelector('#reset').addEventListener('click',fit);
new ResizeObserver(()=>{canvas.width=Math.round(canvas.clientWidth*devicePixelRatio);canvas.height=Math.round(canvas.clientHeight*devicePixelRatio);fit();}).observe(canvas);paint();
</script></html>)html";
    if (!out) throw std::runtime_error("Could not finish writing the HTML file.");
}
}
