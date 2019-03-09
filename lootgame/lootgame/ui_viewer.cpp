#include "stdafx.h"

#include "defs.h"
#include "ui.h"
#include "app.h"

#include "reflection_gen.h"
#include <SDL2/SDL_keycode.h>

static ImGuiWindowFlags BorderlessFlags =
      ImGuiWindowFlags_NoMove |
      ImGuiWindowFlags_NoResize |
      ImGuiWindowFlags_NoTitleBar |
      ImGuiWindowFlags_NoSavedSettings |
      ImGuiWindowFlags_NoScrollWithMouse |
      ImGuiWindowFlags_NoScrollbar |
      ImGuiWindowFlags_NoDocking;


static void _renderViewerFBO(GameState& g, FBO& output, ImVec2 sz) {

   auto rect = getProportionallyFitRect(output.sz, { (int32_t)sz.x, (int32_t)sz.y });

   ImDrawList* draw_list = ImGui::GetWindowDrawList();
   const ImVec2 p = ImGui::GetCursorScreenPos();
   auto a = ImVec2(p.x + rect.x, p.y + rect.y);
   auto b = ImVec2(p.x + rect.x + rect.w, p.y + rect.y + rect.h);

   g.vpScreenArea = { a.x, a.y, b.x - a.x, b.y - a.y };

   draw_list->AddRectFilled(a, b, IM_COL32_BLACK);
   draw_list->AddCallback([](auto, auto) { render::enableSRGB();  }, nullptr);
   draw_list->AddImage((ImTextureID)(intptr_t)output.out[0].handle, a, b);
   draw_list->AddCallback([](auto, auto) { render::disableSRGB(); }, nullptr);
}

static void _showFullScreenViewer(GameInstance& g) {
   auto sz = ImGui::GetIO().DisplaySize;

   auto &style = ImGui::GetStyle();

   auto vp = ImGui::GetMainViewport();

   ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2());
   ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
   ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);

   ImGui::SetNextWindowPos(vp->Pos, ImGuiCond_Always);
   ImGui::SetNextWindowSize(vp->Size, ImGuiCond_Always);

   if (ImGui::Begin("fullscreenViewer", nullptr, BorderlessFlags)) {
      _renderViewerFBO(g.state, g.outputFbo, ImGui::GetContentRegionAvail());
      //g.mouseActive = true;
   }
   ImGui::End();

   ImGui::PopStyleVar(3);

   // handle hotkeys todo: this will be handled by gamestate later
   auto& io = ImGui::GetIO();
   auto& keys = io.KeyMap;
   if (ImGui::IsKeyPressed(keys[ImGuiKey_Escape])) {
      appClose();
   }
   if (io.KeyCtrl && ImGui::IsKeyPressed(keys[ImGuiKey_Enter])) {
      g.state.ui.fullscreen = false;
   }
}

static void _toggleEditing(GameState&g) {
   g.ui.editing = !g.ui.editing;
   LOG(g.ui.editing ? "Entered Edit Mode" : "Existing Edit Mode");
}

static void _setEditMode(GameState& g, GameEditMode mode) {
   switch (g.ui.mode) {
   case GameEditMode_Walls:
      g.ui.editingWall = nullptr;
      break;
   }

   if (g.ui.mode == mode) {
      g.ui.mode = GameEditMode_None;
   }
   else {
      g.ui.mode = mode;
   }

   
}

static bool _wallIsConvex(Wall& w, Float2 mouse) {
   auto &wall = w.points;
   auto polycopy = wall;
   polycopy.push_back(mouse);
   return polyConvex(polycopy.data(), polycopy.size());
}

static bool _pointInWall(Wall& w, Float2 mouse) {
   return pointInPoly(mouse, w.points.data(), w.points.size());
}


static void _viewerMenuBar(GameState& g) {
   if (ImGui::BeginMenuBar()) {
      bool editing = g.ui.editing;
      if (editing) { ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetColorU32(ImGuiCol_ButtonActive)); }
      if (ImGui::Button(ICON_FA_PENCIL_ALT)) { _toggleEditing(g); }
      if (editing) { ImGui::PopStyleColor(); }

      ImGui::Separator();
      ImGui::Checkbox("Game UI", &g.ui.showGameUI);

      ImGui::Separator();

      if (!editing) {
         beginDisabled();
      }

      ImGui::Checkbox("Edit Grid", &g.ui.showEditGrid);
      float gridSize = g.ui.gridSize.x;
      ImGui::PushItemWidth(100.0f);
      ImGui::SliderFloat("##grid", &gridSize, 10.0f, 100.0f, "%0.0f");
      ImGui::PopItemWidth();
      g.ui.gridSize = { gridSize, gridSize };

      ImGui::Separator();

      auto m = g.ui.mode;


      if (m == GameEditMode_Walls) {ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetColorU32(ImGuiCol_ButtonActive));}
      if (ImGui::Button("Walls")) { _setEditMode(g, GameEditMode_Walls); }
      if (m == GameEditMode_Walls) {ImGui::PopStyleColor();}

      if (!editing) {
         endDisabled();
      }

      ImGui::EndMenuBar();
   }
}

static void _statusBar(GameState&g) {

   if (g.ui.editing) {
      ImGui::TextUnformatted("Editing");
      ImGui::SameLine();
   }

   auto &vp = g.camera.viewport;
   ImGui::Text("VP: x:%0.1f y:%0.1f w:%0.1f h:%0.1f", vp.x, vp.y, vp.w, vp.h);

   ImGui::SameLine();

   auto mpos = g.io.mousePos.toWorld();
   ImGui::Text("Mouse (%0.1f, %0.1f)", mpos.x, mpos.y);
}

static void _handleWallInputs(GameState& g) {
   auto& current = g.ui.editingWall;

   if (ImGui::IsItemClicked()) {
      if (!current) {
         Wall newall;
         newall.points.push_back(g.io.mousePos.toWorld());
         g.map.walls.push_back(newall);
         current = &g.map.walls.back();
      }
      else {
         auto mouse = g.io.mousePos.toWorld();
         if (_wallIsConvex(*current, mouse)) {
            current->points.push_back(mouse);

            Float2 min = { FLT_MAX,FLT_MAX }, max = { -FLT_MAX,-FLT_MAX };
            for (auto&&p : current->points) {
               min.x = MIN(min.x, p.x);
               min.y = MIN(min.y, p.y);
               max.x = MAX(max.x, p.x);
               max.y = MAX(max.y, p.y);
            }
            current->bb = rectFromMinMax(min, max);
         }         
      }
   }
}



static void _viewerHandleInput(GameState& g) {
   auto& io = ImGui::GetIO();
   auto& keys = io.KeyMap;

   if (ImGui::IsKeyPressed(ImGui::GetIO().KeyMap[ImGuiKey_Escape])) {
      appClose();
   }
   if (io.KeyCtrl && ImGui::IsKeyPressed(keys[ImGuiKey_Enter])) {
      g.ui.fullscreen = true;
      g.ui.editing = false;
   }
   if (ImGui::IsKeyPressed(SDL_SCANCODE_E)) {
      _toggleEditing(g);
   }

   if (g.ui.editing) {

      //------------- camera drag with space
      if (ImGui::IsWindowHovered() && ImGui::IsKeyPressed(keys[ImGuiKey_Space])) {
         g.ui.cameraDragStart = g.io.mousePos.toWorld();
         g.ui.cameraDragVpStart = g.camera.viewport.xy();
         g.ui.draggingCamera = true;
      }
      if (g.ui.draggingCamera) {
         auto start = g.ui.cameraDragStart;
         auto end = g.io.mousePos.toWorld();
         Float2 delta = { start.x - end.x, start.y - end.y };

         auto&vp = g.camera.viewport;
         vp.setPos(vp.xy() + delta);

         if (ImGui::IsKeyReleased(keys[ImGuiKey_Space])) {
            g.ui.draggingCamera = false ;
         }
      }

      // ----------------- camera zoom with mouse
      if (!g.ui.draggingCamera) {
         auto wheel = ImGui::GetIO().MouseWheel;
         if (wheel != 0.0f) {

            auto mPos = g.io.mousePos.toWorld();
            auto scale = 1.0f + -wheel * 0.1f;

            auto& vp = g.camera.viewport;
            if ( scale > 1.0f || vp.w > 50.0f) {
               vp.scaleFromPoint(mPos, scale);
            }

            
         }
      }

      switch (g.ui.mode) {
      case GameEditMode_Walls:
         _handleWallInputs(g);
         break;
      }
   }
}

static void _doRightClickMenu(GameState&g) {

   if (ImGui::MenuItem("Spawn Test Dude")) {
      DEBUG_gameSpawnDude(g, g.io.mousePos);
   }
   if (ImGui::MenuItem("Spawn 100 Test Dudes")) {
      for(int i = 0; i < 10; ++i)
         for (int j = 0; j < 10; ++j) {
            auto coords = g.io.mousePos;

            auto p = coords.toWorld();
            p.x += i * 40;
            p.y += j * 40;

            DEBUG_gameSpawnDude(g, Coords::fromWorld(p));
         }
         
   }
   ImGui::Separator();

   switch (g.ui.mode) {
   case GameEditMode_None:
      if (ImGui::MenuItem("Edit Walls")) {
         if (!g.ui.editing) {
            _toggleEditing(g);
         }
         _setEditMode(g, GameEditMode_Walls);
      }
   case GameEditMode_Walls:
      if (g.ui.editingWall) {
         if (ImGui::MenuItem("Finish Wall")) {
            auto& w = g.ui.editingWall;
            if (w->points.size() < 3) {
               for (auto iter = g.map.walls.begin(); iter != g.map.walls.end(); ++iter) {
                  if (&*iter == w) {
                     g.map.walls.erase(iter);
                     break;
                  }
               }
            }
            g.ui.editingWall = nullptr;
         }
      }
      break;
   }
}


static void _renderGrid(GameState& g) {
   Float2 &gridSize = g.ui.gridSize;

   auto a = g.vpScreenArea.Min();
   auto b = g.vpScreenArea.Max();
   ImGui::GetWindowDrawList()->AddRect(a, b, IM_COL32(255, 255, 255, 128), 0.0f, 15, 2.0f);

   auto& vp = g.camera.viewport;
   auto vpMin = vp.Min();
   auto vpMax = vp.Max();

   vpMin.x = floorf(vpMin.x / gridSize.x) * gridSize.x;
   vpMin.y = floorf(vpMin.y / gridSize.y) * gridSize.y;

   vpMin.x = clamp(vpMin.x, 0, vp.Max().x);
   vpMin.y = clamp(vpMin.y, 0, vp.Max().y);
   vpMax.x = clamp(vpMax.x, 0, g.map.size.x);
   vpMax.y = clamp(vpMax.y, 0, g.map.size.y);

   auto color = IM_COL32(255, 255, 255, 64);

   while (vpMin.x < vpMax.x) {
      Coords pa = { { vpMin.x, vp.y } };
      Coords pb = { { vpMin.x, vp.y + vp.h } };
      auto a = pa.toScreen(g);
      auto b = pb.toScreen(g);
      ImGui::GetWindowDrawList()->AddLine(ImVec2(a.x, a.y), ImVec2(b.x, b.y), color);
      vpMin.x += gridSize.x;
   }
   while (vpMin.y < vpMax.y) {
      Coords pa = { { vp.x, vpMin.y } };
      Coords pb = { { vp.x + vp.w, vpMin.y } };
      auto a = pa.toScreen(g);
      auto b = pb.toScreen(g);
      ImGui::GetWindowDrawList()->AddLine(ImVec2(a.x, a.y), ImVec2(b.x, b.y), color);
      vpMin.y += gridSize.y;
   }

}

static void _renderPhyObjs(GameState& g) {
   for (auto&& p : g.phySys.objs) {
      if (p->type == PhyObject::PhyType_Circle) {
         auto center = Coords::fromWorld(p->pos).toScreen(g);
         auto radius = Coords::fromWorld({ p->circle.size,  p->circle.size }).toScreen(g) - Coords::fromWorld({ 0,0 }).toScreen(g);
         ImGui::GetWindowDrawList()->AddCircle(center, radius.x, IM_COL32_WHITE);

         Float2 a = { center.x - 10, center.y };
         Float2 b = { center.x + 10, center.y };
         Float2 c = { center.x, center.y - 10 };
         Float2 d = { center.x, center.y + 10 };
         ImGui::GetWindowDrawList()->AddLine(a, b, IM_COL32_WHITE);
         ImGui::GetWindowDrawList()->AddLine(c, d, IM_COL32_WHITE);

      }
   }

   for (auto&& wall : g.map.walls) {
      auto pCount = wall.points.size();
      ImU32 lineCol = IM_COL32(50, 200, 50, 255);
      ImU32 badPolyCol = IM_COL32(255, 0, 0, 255);

      for (auto iter = wall.points.begin(); iter != wall.points.end(); ++iter) {
         auto c = lineCol;

         auto next = iter + 1;
         if (next == wall.points.end()) {
            next = wall.points.begin();
         }

         auto a = Coords::fromWorld(*iter).toScreen(g);
         auto b = Coords::fromWorld(*next).toScreen(g);

         auto norm = v2Perp(v2Normalized(b - a));

         auto radius = Coords::fromWorld({ 10,10 }).toScreen(g) - Coords::fromWorld({ 0,0 }).toScreen(g);
         norm = { norm.x * radius.x, norm.y * radius.y };

         auto guideCol = IM_COL32(255, 255, 255, 128);

         auto oa = a;
         auto ob = b;
         oa += norm;
         ob += norm;
         ImGui::GetWindowDrawList()->AddLine(oa, ob, guideCol);

         oa = a; ob = b;
         oa -= norm;
         ob -= norm;
         ImGui::GetWindowDrawList()->AddLine(oa, ob, guideCol);

         ImGui::GetWindowDrawList()->AddCircle(a, radius.x, guideCol);
         ImGui::GetWindowDrawList()->AddCircle(b, radius.x, guideCol);
      }
   }
}

static void _renderWalls(GameState& g) {
   ImU32 lineCol = IM_COL32(50, 200, 50, 255);
   ImU32 badPolyCol = IM_COL32(255, 0, 0, 255);
   ImU32 bbCol = IM_COL32(0, 255, 255, 128);

   ImU32 hovCol = IM_COL32(255, 255, 0, 255);

   auto mouse = g.io.mousePos.toWorld();

   bool badPoly = false;
   if (g.ui.editingWall) {
      badPoly = !_wallIsConvex(*g.ui.editingWall, mouse);
   }

   for (auto&& wall : g.map.walls) {
      auto pCount = wall.points.size();

      for (auto iter = wall.points.begin(); iter != wall.points.end(); ++iter) {
         auto c = lineCol;
         bool editing = &wall == g.ui.editingWall;

         if (badPoly && editing) {
            c = badPolyCol;
         }

         auto next = iter + 1;
         if (next == wall.points.end()) {
            next = wall.points.begin();
         }

         auto a = Coords::fromWorld(*iter).toScreen(g);
         auto b = Coords::fromWorld(*next).toScreen(g);
         
         ImGui::GetWindowDrawList()->AddLine(a, b, c);
      }
   }

   // render the editing wall
   if (g.ui.editingWall) {

      auto &eWall = g.ui.editingWall->points;
      auto eCount = eWall.size();

      auto c = badPoly ? badPolyCol : lineCol;

      if (eCount > 0) {
         ImGui::GetWindowDrawList()->AddLine(
            Coords::fromWorld(eWall.back()).toScreen(g),
            g.io.mousePos.toScreen(g), c);
      }

      if (eCount > 1) {
         ImGui::GetWindowDrawList()->AddLine(
            Coords::fromWorld(eWall.front()).toScreen(g),
            g.io.mousePos.toScreen(g), c);
      }
   }
}

struct ShadowCasterSegment {
   Float2 a, b;   
   bool aclipped, bclipped;
   float adistSq = 0.0f, bdistSq = 0.0f;

   int asegment = -1, bsegment = -1;
};

enum ShadowRayType {
   ShadowRayType_LeftAligned = 0,
   ShadowRayType_RightAligned,
   ShadowRayType_Straddling
};

struct ShadowCastRay {
   Float2 p;
   float dist;
   int asegment = -1, bsegment = -1;
   Float2 asegEnd, bsegEnd;
   int asegSide, bsegSide;
   ShadowRayType type;

   Float2 dir;
   float anglef;

   bool onEdge = false;
};

enum ShadowActiveNodeType {
   ShadowActiveNode_EndPoint,
   ShadowActiveNode_OnSegment,
   ShadowActiveNode_OnEdge
};

struct ShadowActiveNode {
   Float2 p;
   float dist;
   
   ShadowActiveNodeType type;
   int segment;
   Float2 segmentEnd; // clockwise endpoint of active segment 
   float segEndDist;
};

static Float2 _clipSegment(Float2 inside, Float2 outside, Float2 cpos, float radius) {

   // cast from b to a to intersect sphere
   auto segdiff = inside - outside;
   auto seglen = v2Len(segdiff);
   auto segdir = segdiff / seglen;

   float t; Float2 q;
   intersectRaySphere(outside, segdir, cpos, radius, t, q);
   return q;
}

static void _solveShadowCasts(Array<ShadowCasterSegment> &segments, Array<ShadowCastRay> &rays, Float2 ppos, float radius);

static void _renderShadowCalc(GameState& g) {
   const float SightRadius = 200;

   auto world0 = Coords::fromWorld({ 0,0 }).toScreen(g);
   auto ppos = Coords::fromWorld(g.maindude.phy.pos).toScreen(g);
   auto radius = (Coords::fromWorld({ SightRadius,  SightRadius }).toScreen(g) - world0).x;
   auto radiusSq = radius * radius;

   auto drawlist = ImGui::GetWindowDrawList();

   auto radiusCol = IM_COL32_WHITE;
   auto segmentCol = IM_COL32(255, 127, 39, 255);
   auto segmentCol2 = IM_COL32(255, 255, 128, 128);
   auto rayCol = IM_COL32(255, 0, 0, 128);

   drawlist->AddCircle(ppos, radius, radiusCol, 50);

   Array<ShadowCasterSegment> segments, orphanedSegments;

   // clip all the walls and build the segments list
   for (auto&& w : g.map.walls) {
      auto segStart = segments.size();

      for (auto iter = w.points.begin(); iter != w.points.end(); ++iter) {
         auto next = iter + 1;
         if (next == w.points.end())next = w.points.begin();

         auto a = Coords::fromWorld(*iter).toScreen(g);
         auto b = Coords::fromWorld(*next).toScreen(g);

         auto adistSq = v2LenSquared(a - ppos);
         auto bdistSq = v2LenSquared(b - ppos);

         bool ain = adistSq < radiusSq;
         bool bin = bdistSq < radiusSq;

         if (ain && bin) {
            // whole segment in range
            segments.push_back({ a, b, false, false, adistSq, bdistSq });
         }
         else if (!ain && !bin) {
            auto segdiff = b - a;
            auto seglen = v2Len(segdiff);
            auto segdir = segdiff / seglen;

            float t; Float2 qa, qb;
            if (intersectRaySphere(a, segdir, ppos, radius, t, qa) &&
               intersectRaySphere(b, -segdir, ppos, radius, t, qb)) {

               orphanedSegments.push_back({ qa, qb, true, true, v2LenSquared(ppos - qa), v2LenSquared(ppos - qb), -1, -1 });
            }           
         }
         else {
            if (bin) {
               a = _clipSegment(b, a, ppos, radius);
               segments.push_back({ a, b, true, false, v2LenSquared(a - ppos), bdistSq });
            }
            else {
               b = _clipSegment(a, b, ppos, radius);
               segments.push_back({ a, b, false, true, adistSq, v2LenSquared(b - ppos) });
            }
         }
      }
   
      // now we can go through the segments made by this wall and
      // link up the segments they're attached to
      auto segCount = segments.size() - segStart;
      if (segCount > 0) {
         for (size_t s = segStart; s < segStart + segCount; ++s) {
            if (!segments[s].aclipped) {
               segments[s].asegment = s - 1;
            }
            if (!segments[s].bclipped) {
               segments[s].bsegment = s + 1;
               if (segments[s].bsegment >= segStart + segCount) {
                  segments[s].bsegment = segStart;
                  segments[segStart].asegment = s;

                  
               }
            }
         }
      }
   }

   segments.push_back_array(orphanedSegments);

   // render out all our segment index references
   int i = 0;
   for (auto &segment : segments) {
      drawlist->AddLine(segment.a, segment.b, segmentCol, 2.0f);

   //   auto lblPos = segment.a + ((segment.b - segment.a) * 0.5f) + Float2{0, -30};
   //   drawlist->AddText(lblPos, segmentCol, format("%d", i).c_str());

   //   drawlist->AddText(segment.a + Float2{ 0, -12 }, segmentCol2, format("%d", segment.asegment).c_str());
   //   drawlist->AddText(segment.b, segmentCol2, format("%d", segment.bsegment).c_str());
      ++i;
   }

   // now we build our ray list
   Array<ShadowCastRay> rays;

   i = 0;
   for (auto &segment : segments) {
      bool hasAConnect = segment.asegment >= 0;
      bool hasBConnect = segment.bsegment >= 0;

      if (!hasAConnect && !hasBConnect) {
         rays.push_back({ segment.a, sqrtf(segment.adistSq), -1, i });
         rays.push_back({ segment.b, sqrtf(segment.bdistSq), i, -1 });
      }
      else if (!hasAConnect) {
         rays.push_back({segment.a, sqrtf(segment.adistSq), -1, i});
      }
      else if (hasAConnect && hasBConnect) {
         rays.push_back({ segment.a, sqrtf(segment.adistSq), segment.asegment, i });
      }
      else {
         rays.push_back({ segment.a, sqrtf(segment.adistSq), segment.asegment, i });
         rays.push_back({ segment.b, sqrtf(segment.bdistSq), i, -1 });
      }

      ++i;
   }

   // post process on rays, calculate their angles and determine their orientation
   for (auto&& ray : rays) {

      if (ray.asegment >= 0) {
         ray.asegEnd = segments[ray.asegment].a;
         ray.asegSide = pointSideOfSegment(ppos, ray.p, ray.asegEnd);
      }
      if (ray.bsegment >= 0) {
         ray.bsegEnd = segments[ray.bsegment].b;
         ray.bsegSide = pointSideOfSegment(ppos, ray.p, ray.bsegEnd);
      }

      if (ray.asegment >= 0 && ray.bsegment >= 0) {
         if (ray.asegSide == ray.bsegSide) {
            ray.type = ray.asegSide >= 0 ? ShadowRayType_LeftAligned : ShadowRayType_RightAligned;
         }
         else {
            ray.type = ShadowRayType_Straddling;
         }
      }
      else if (ray.asegment >= 0) {
         ray.type = ray.asegSide >= 0 ? ShadowRayType_LeftAligned : ShadowRayType_RightAligned;
      }
      else if (ray.bsegment >= 0) {
         ray.type = ray.bsegSide >= 0 ? ShadowRayType_LeftAligned : ShadowRayType_RightAligned;
      }

      ray.dir = (ray.p - ppos) / ray.dist;
      ray.anglef = v2Angle(ray.dir);
   }

   // sort in polar order
   std::sort(rays.begin(), rays.end(), [](ShadowCastRay const&a, ShadowCastRay const&b) {return a.anglef < b.anglef; });


   // now we need to determine our first ray, its the first raw that doesnt intersect any segments
   int clearRay = -1;
   for (int i = 0; i < rays.size(); ++i) {
      auto&ray = rays[i];
      bool intersect = false;
      for (auto s = 0; s < segments.size(); ++s) {
         if (s == ray.asegment || s == ray.bsegment) {
            continue;
         }
         auto&seg = segments[s];
         float u, t; Float2 q;
         if (segmentSegmentIntersect(ppos, ray.p, seg.a, seg.b, u, t, q)) {
            intersect = true;
            break;
         }
      }
      if (!intersect) {
         clearRay = i;
         break;
      }
   }

   if (clearRay < 0) {
      return;
   }

   std::rotate(rays.begin(), rays.begin() + clearRay, rays.end());

   // draw our rays
   //drawlist->AddText(ppos + Float2{ 20,20 }, rayCol, format("%d Rays", rays.size()).c_str());
   //i = 0;
   //auto rCount = rays.size();
   //for (auto&& ray : rays) {
   //   auto ImDrawCornerFlags_Left = IM_COL32(255, 0, 0, 192);
   //   auto ImDrawCornerFlags_Right = IM_COL32(0, 255, 0, 192);
   //   auto ImDrawCornerFlags_Straddle = IM_COL32(0, 0, 255, 192);

   //   //drawlist->AddCircle(ray.p, 10, rayCol);
   //   unsigned int c = 0;
   //   switch (ray.type) {
   //   case ShadowRayType_LeftAligned: c = ImDrawCornerFlags_Left; break;
   //   case ShadowRayType_RightAligned: c = ImDrawCornerFlags_Right; break;
   //   case ShadowRayType_Straddling: c = ImDrawCornerFlags_Straddle; break;
   //   }

   //   if (i == 0) {
   //      drawlist->AddLine(ray.p, ppos, c, 4.0f);
   //   }
   //   else {
   //      drawlist->AddLine(ray.p, ppos, c);
   //   }

   //   ++i;
   //}

   _solveShadowCasts(segments, rays, ppos, radius);

}

static int _rayInnerSegment(ShadowCastRay&ray, Float2 ppos) {
   if (ray.asegment < 0) { return ray.bsegment; }
   if (ray.bsegment < 0) { return ray.asegment; }
   if (ray.asegment < 0 && ray.bsegment < 0) { return -1; }
   
   if (ray.type == ShadowRayType_LeftAligned) {
      // both head right, if the determinate between them is positive
      bool aInner = v2Determinant(ray.asegEnd - ray.p, ray.bsegEnd - ray.p) < 0;
      return aInner ? ray.asegment : ray.bsegment;
   }
   else {
      // return the one thats on the right
      return ray.asegSide >= 0 ? ray.asegment : ray.bsegment;
   }
}

static void _commitTriangle(Float2 a, Float2 b, Float2 c) {
   ImVec2 poly[3] = { a, b, c };
   ImGui::GetWindowDrawList()->AddConvexPolyFilled(poly, 3, IM_COL32(0, 150, 255, 64));
}

static void _segmentClockwiseEnd(ShadowCasterSegment& segment, Float2 ppos, Float2&endOut) {
   if (v2Determinant(segment.a - ppos, segment.b - ppos) < 0) {
      endOut = segment.a;
   }
   else {
      endOut = segment.b;
   }

}


 void _solveShadowCasts(Array<ShadowCasterSegment> &segments, Array<ShadowCastRay> &rays, Float2 ppos, float radius) {
    auto drawlist = ImGui::GetWindowDrawList();

    ShadowActiveNode active;
    auto &firstRay = rays[0];

    Array<int> openList;

    // use first ray to setup our initial node


    // any segments the ray intersects needs to be added to the open lis
    int shortestSegment = -1;
    float shortestT = FLT_MAX;
    Float2 shortestPoint;

    auto endPoint = ppos + firstRay.dir * radius;

    for (int s = 0; s < segments.size(); ++s) {
       if (s == firstRay.asegment || s == firstRay.bsegment) {
          continue;
       }
       auto& seg = segments[s];

       float u, t; Float2 q;
       if (segmentSegmentIntersect(ppos, endPoint, seg.a, seg.b, u, t, q)) {
          openList.push_back(s);
          if (t < shortestT) {
             shortestT = t;
             shortestSegment = s;
             shortestPoint = q;
          }
       }
    }
    

    if (firstRay.type == ShadowRayType_RightAligned) {
       if (shortestSegment >= 0) {
          openList.erase(std::find(openList.begin(), openList.end(), shortestSegment));

          active.p = shortestPoint;
          active.dist = v2Dist(active.p, ppos);
          active.segment = shortestSegment;
          active.type = ShadowActiveNode_OnSegment;

          _segmentClockwiseEnd(segments[active.segment], ppos, active.segmentEnd);
       }
       else {
          active.p = endPoint;
          active.dist = radius;
          active.segment = shortestSegment;
          active.type = ShadowActiveNode_OnEdge;

          _segmentClockwiseEnd(segments[active.segment], ppos, active.segmentEnd);
       }
    }
    else {
       active.p = firstRay.p;
       active.dist = firstRay.dist;
       active.segment = _rayInnerSegment(firstRay, ppos);
       active.type = ShadowActiveNode_EndPoint;

       _segmentClockwiseEnd(segments[active.segment], ppos, active.segmentEnd);

       if (firstRay.type == ShadowRayType_LeftAligned) {
          auto otherSeg = -1;
          if (firstRay.asegment >= 0 && firstRay.asegment != active.segment) { otherSeg = firstRay.asegment; }
          else if (firstRay.bsegment >= 0 && firstRay.bsegment != active.segment) { otherSeg = firstRay.bsegment; }
          if (otherSeg >= 0) {
             openList.push_back(otherSeg);
          }
          
       }

       // push the other segment onto open
       //if (firstRay.asegment >= 0 && firstRay.asegment != active.segment) { openList.push_back(firstRay.asegment); }
       //else if (firstRay.bsegment >= 0 && firstRay.bsegment != active.segment) { openList.push_back(firstRay.bsegment); }
    }

    // helper draw out our active node and open List
    //drawlist->AddCircle(active.p, 5, IM_COL32(0, 255, 255, 255));
    //drawlist->AddLine(segments[active.segment].a, segments[active.segment].b, IM_COL32(0, 255, 255, 128), 3);
    //for (auto open : openList) {
    //   drawlist->AddLine(segments[open].a, segments[open].b, IM_COL32(255, 255, 255, 128), 3);
    //}

    static int displayCount = 0;
    static int clock = 0;

    //if (clock++ > 50) {
    //   clock -= 50;
    //   ++displayCount;
    //}

    static bool frameStepping = false;

    if (frameStepping && ImGui::IsKeyPressed(ImGui::GetIO().KeyMap[ImGuiKey_LeftArrow])) {
       --displayCount;
    }
    if (frameStepping && ImGui::IsKeyPressed(ImGui::GetIO().KeyMap[ImGuiKey_RightArrow])) {
       ++displayCount;
    }
    if (ImGui::IsKeyPressed(ImGui::GetIO().KeyMap[ImGuiKey_DownArrow])) {
       frameStepping = !frameStepping;
       displayCount = 0;
    }

    // finally we actually iterate over the rays
    int iteration = 0;

    for (auto rayiter = rays.begin() + 1; ; ++rayiter) {
       if (frameStepping) {
          auto stoppingPoint = displayCount % rays.size();
          if (iteration++ >= stoppingPoint) {
             //if (rayiter != rays.end() || iteration < stoppingPoint) {
                break;
             //}
          }
       }

       bool finish = false;
       if (rayiter == rays.end()) {
          rayiter = rays.begin();
          finish = true;
       }
       auto&ray = *rayiter;

       

       // if active and ray are both on circle's edge
       if (active.type == ShadowActiveNode_OnEdge && ray.onEdge) {
          _commitTriangle(ppos, active.p, ray.p);
          active.p = ray.p;
          active.dist = ray.dist;
          active.segment = _rayInnerSegment(ray, ppos);
          active.type = ShadowActiveNode_OnEdge;
          _segmentClockwiseEnd(segments[active.segment], ppos, active.segmentEnd);

       }
       // if active is on edge and ray is not
       else if (active.type == ShadowActiveNode_OnEdge) {
          

          _commitTriangle(ppos, active.p, ppos + ray.dir * radius);
          active.p = ray.p;
          active.dist = ray.dist;
          active.segment = _rayInnerSegment(ray, ppos);
          active.type = ShadowActiveNode_EndPoint;
          _segmentClockwiseEnd(segments[active.segment], ppos, active.segmentEnd);

          int otherSeg = -1;
          if (ray.asegment >= 0 && ray.asegment != active.segment) { otherSeg = ray.asegment; }
          else if (ray.bsegment >= 0 && ray.bsegment != active.segment) { otherSeg = ray.bsegment; }
          if (otherSeg >= 0) {
             auto found = std::find(openList.begin(), openList.end(), otherSeg);
             if (found != openList.end()) {
                openList.erase(found);
             }
             else {
                openList.push_back(otherSeg);
             }
          }

       }
       // if ray is on edge and active is not
       else if (ray.onEdge) {

       }
       // ray closes active segment
       else if (ray.asegment == active.segment || ray.bsegment == active.segment) {
          _commitTriangle(ppos, active.p, ray.p);
          auto otherSeg = -1;
          if (ray.asegment >= 0 && ray.asegment != active.segment) { otherSeg = ray.asegment; }
          else if (ray.bsegment >= 0 && ray.bsegment != active.segment) { otherSeg = ray.bsegment; }
          if (otherSeg >= 0) {
             auto found = std::find(openList.begin(), openList.end(), otherSeg);
             if (found != openList.end()) {
                openList.erase(found);
             }
             else {
                active.p = ray.p;
                active.dist = ray.dist;
                active.segment = otherSeg;
                active.type = ShadowActiveNode_EndPoint;
                _segmentClockwiseEnd(segments[active.segment], ppos, active.segmentEnd);
             }
          }
          if (ray.type == ShadowRayType_RightAligned) {
             int shortestSegment = -1;
             float shortestT = FLT_MAX;
             Float2 shortestPoint;

             ;

             auto endPoint = ppos + ray.dir *radius;
             //drawlist->AddCircle(endPoint, 10, IM_COL32(100, 0, 100, 255), 12, 5.0f);

             for (auto s : openList) {
                if (s == ray.asegment || s == ray.bsegment) {
                   continue;
                }
                auto& seg = segments[s];
                float u, t; Float2 q;
                if (segmentSegmentIntersect(ppos, endPoint, seg.a, seg.b, u, t, q)) {
                   if (t < shortestT) {
                      shortestT = t;
                      shortestSegment = s;
                      shortestPoint = q;
                   }
                }
             }

             if (shortestSegment >= 0) {
                openList.erase(std::find(openList.begin(), openList.end(), shortestSegment));
                active.p = shortestPoint;
                active.dist = v2Dist(active.p, ppos);
                active.segment = shortestSegment;
                active.type = ShadowActiveNode_OnSegment;
                _segmentClockwiseEnd(segments[active.segment], ppos, active.segmentEnd);
             }
             else {
                active.p = endPoint;
                
                active.dist = radius;
                active.segment = -1;
                active.type = ShadowActiveNode_OnEdge;
             }
          }
       }
       else {
          //drawlist->AddCircle(ray.p, 10, IM_COL32(255, 255, 255, 255));
          //drawlist->AddCircle(active.segmentEnd, 10, IM_COL32(255, 0, 128, 255));

          bool rayBehind = v2Determinant(ray.p - active.p, active.segmentEnd - active.p) >= 0;

          // ray.p is closer to center than active.p
          if (!rayBehind) {

             auto endPoint = ppos + ray.dir * radius;
             auto &activeSegment = segments[active.segment];

             float u, t; Float2 q;
             segmentSegmentIntersect(ppos, endPoint, activeSegment.a, activeSegment.b, u, t, q);

             _commitTriangle(ppos, active.p, q);
             openList.push_back(active.segment);

             active.p = ray.p;
             active.dist = ray.dist;
             active.segment = _rayInnerSegment(ray, ppos);
             active.type = ShadowActiveNode_EndPoint;
             _segmentClockwiseEnd(segments[active.segment], ppos, active.segmentEnd);

             int otherSeg = -1;
             if (ray.asegment >= 0 && ray.asegment != active.segment) { otherSeg = ray.asegment; }
             else if (ray.bsegment >= 0 && ray.bsegment != active.segment) { otherSeg = ray.bsegment; }
             if (otherSeg >= 0) {
                auto found = std::find(openList.begin(), openList.end(), otherSeg);
                if (found != openList.end()) {
                   openList.erase(found);
                }
                else {
                   openList.push_back(otherSeg);
                }
             }
             

          }
          // ray.p is further from center than active.p
          else {   
             if (ray.asegment >= 0) {
                auto found = std::find(openList.begin(), openList.end(), ray.asegment);
                if (found != openList.end()) {
                   openList.erase(found);
                }
                else {
                   openList.push_back(ray.asegment);
                }
             }
             if (ray.bsegment >= 0) {
                auto found = std::find(openList.begin(), openList.end(), ray.bsegment);
                if (found != openList.end()) {
                   openList.erase(found);
                }
                else {
                   openList.push_back(ray.bsegment);
                }
             }
          }
       }

       if (finish) {
          openList.clear();
          break;
       }
    }
    
    for (auto o : openList) {
       auto os = segments[o];
       drawlist->AddLine(os.a, os.b, IM_COL32(255, 255, 255, 200), 5.0f);
    }
    
    if (frameStepping) {

       auto r = rays[displayCount % rays.size()];
       drawlist->AddLine(ppos, r.p, IM_COL32(255, 128, 198, 255), 7.0f);

       auto &as = segments[active.segment];
       drawlist->AddLine(as.a, as.b, IM_COL32(255, 0, 198, 192), 7.0f);

       for (auto&& r : rays) {

       }
    }
    


    
}

static void _renderHelpers(GameState& g) {
   if (g.ui.editing && g.ui.showEditGrid) {
      _renderGrid(g);
      _renderWalls(g);
      //_renderPhyObjs(g);

      _renderShadowCalc(g);
   }
}


static bool _showWindowedViewer(GameInstance& gi) {
   bool p_open = true;
   auto& g = gi.state;

   if (ImGui::Begin(gi.winTitle.c_str(), &p_open, ImGuiWindowFlags_MenuBar)) {
      g.ui.focused = ImGui::IsWindowFocused();

      _viewerMenuBar(g);

      auto viewsz = ImGui::GetContentRegionAvail();
      viewsz.y -= ImGui::GetTextLineHeightWithSpacing();

      _renderViewerFBO(g, gi.outputFbo, viewsz);
      ImGui::InvisibleButton("invisbtn", viewsz);
      if (ImGui::BeginPopupContextItem()) {
         _doRightClickMenu(g);
         ImGui::EndPopup();
      }

      if (g.ui.focused) {
         _viewerHandleInput(g);
      }

      auto a = g.vpScreenArea.Min();
      auto b = g.vpScreenArea.Max();      
      
      ImGui::PushClipRect(a, b, false);      
      _renderHelpers(g);
      ImGui::PopClipRect();

      _statusBar(g);

   }
   ImGui::End();

   if (!p_open) {
      //cleanup?
   }

   return p_open;
}


void uiDoGameDebugger(GameInstance& instance) {
   if (ImGui::Begin("Game Debugger", nullptr)) {
      auto&g = instance.state;

      if (ImGui::Button("Restart", ImVec2(ImGui::GetContentRegionAvailWidth(), 0))) {
         instance.state = {};
         gameStartActionMode(instance.state);
      }

      if (ImGui::Button("Reload Shaders", ImVec2(ImGui::GetContentRegionAvailWidth(), 0))) {
         //gameReloadShaders(g);
      }
      if (ImGui::Button("Spawn Dude", ImVec2(ImGui::GetContentRegionAvailWidth(), 0))) {
         DEBUG_gameSpawnDude(g, Coords::fromWorld(Float2{(float)(rand()%100), (float)(rand()%100)}));
      }
      if (ImGui::Button("Spawn 100 Dudes", ImVec2(ImGui::GetContentRegionAvailWidth(), 0))) {
         for (int i = 0; i < 100; ++i) {
            DEBUG_gameSpawnDude(g, Coords::fromWorld(Float2{ (float)(rand() % 100), (float)(rand() % 100) }));
         }
         
      }

      auto&io = g.io;

      ImGui::SetNextTreeNodeOpen(true, ImGuiCond_Appearing);
      if (ImGui::CollapsingHeader("Input")) {
         ImGui::Text("Left Stick: (%f, %f)", io.leftStick.x, io.leftStick.y);
         ImGui::Text("Right Stick: (%f, %f)", io.rightStick.x, io.rightStick.y);
         ImGui::Text("Left Trigger: %f", io.leftTrigger);
         ImGui::Text("Right Trigger: %f", io.rightTrigger);

         ImGui::Text("LeftTrigger: %s", io.buttonDown[GameButton_LT] ? "Down" : "Up");
         ImGui::Text("Pressed: %s", io.buttonPressed[GameButton_LT] ? "true" : "false");
         ImGui::Text("Released: %s", io.buttonReleased[GameButton_LT] ? "true" : "false");
      }

      doTypeUI(&g.DEBUG);

   }
   ImGui::End();
}



bool gameDoUIWindow(GameInstance& inst) {
   if (inst.state.ui.fullscreen) {
      _showFullScreenViewer(inst);
      return true;
   }
   return _showWindowedViewer(inst);   
}
