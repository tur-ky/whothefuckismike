#include "imgui_custom_widgets.hpp"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <imgui_internal.h>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
static bool s_DC = false, s_PDC = false;

ImFont *g_BoldFont = nullptr;

ImFont *g_HeaderFont = nullptr;

namespace UI
{
void ApplyBrutalistTheme()
{
    ImGuiStyle &s = ImGui::GetStyle();

    s.WindowRounding = s.ChildRounding = s.FrameRounding = s.PopupRounding = s.ScrollbarRounding = s.GrabRounding =
        s.TabRounding = 0;

    s.ItemSpacing = ImVec2(6, 4);
    s.FramePadding = ImVec2(4, 3);
    s.WindowPadding = ImVec2(6, 4);

    ImVec4 *c = s.Colors;

    c[ImGuiCol_WindowBg] = ImVec4(.118f, .118f, .118f, 1);
    c[ImGuiCol_ChildBg] = ImVec4(.15f, .15f, .15f, 1);

    c[ImGuiCol_Text] = ImVec4(1, 1, 1, 1);
    c[ImGuiCol_TextDisabled] = ImVec4(.627f, .627f, .627f, 1);

    c[ImGuiCol_Button] = ImVec4(.2f, .2f, .2f, 1);
    c[ImGuiCol_ButtonHovered] = ImVec4(.28f, .28f, .28f, 1);
    c[ImGuiCol_ButtonActive] = ImVec4(.9f, .08f, 0, 1);

    c[ImGuiCol_FrameBg] = ImVec4(.1f, .1f, .1f, 1);
    c[ImGuiCol_FrameBgHovered] = ImVec4(.18f, .18f, .18f, 1);
    c[ImGuiCol_FrameBgActive] = ImVec4(.22f, .22f, .22f, 1);

    c[ImGuiCol_Border] = ImVec4(.35f, .35f, .35f, .6f);
    c[ImGuiCol_PopupBg] = ImVec4(.14f, .14f, .14f, .95f);
    c[ImGuiCol_Separator] = ImVec4(.3f, .3f, .3f, 1);
}
static bool RCP(const char *l, float *v)
{
    bool c = false;
    char id[64];
    snprintf(id, 64, "##p_%s", l);
    if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(1))
        ImGui::OpenPopup(id);
    if (ImGui::BeginPopup(id))
    {
        if (ImGui::InputFloat("##v", v, 0, 0, "%.2f", ImGuiInputTextFlags_EnterReturnsTrue))
        {
            c = true;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    return c;
}

// Rotated text (-90deg, reads bottom-to-top, tilt head right)
static void DrawRotText(ImDrawList *dl, ImFont *font, ImVec2 pos, const char *text, ImU32 col)
{
    float sz = font->LegacySize;
    ImFontBaked *bk = font->GetFontBaked(sz);
    if (!bk)
        return;

    float cursor = 0;
    const char *p = text;

    while (*p)
    {
        ImFontGlyph *g = bk->FindGlyph((ImWchar)(unsigned char)*p);

        if (g)
        {
            float x0 = g->X0 + cursor, x1 = g->X1 + cursor, y0 = g->Y0, y1 = g->Y1;

            dl->AddImageQuad(font->OwnerAtlas->TexRef, ImVec2(pos.x + y0, pos.y - x0), ImVec2(pos.x + y0, pos.y - x1),
                             ImVec2(pos.x + y1, pos.y - x1), ImVec2(pos.x + y1, pos.y - x0), ImVec2(g->U0, g->V0),
                             ImVec2(g->U1, g->V0), ImVec2(g->U1, g->V1), ImVec2(g->U0, g->V1), col);

            cursor += g->AdvanceX;
        }
        p++;
    }
}

bool CircularKnob(const char *label, float *v, float v_min, float v_max, const char *fmt)
{
    ImGuiIO &io = ImGui::GetIO();
    ImDrawList *dl = ImGui::GetWindowDrawList();

    float R = 22;
    ImVec2 p = ImGui::GetCursorScreenPos(), cn = ImVec2(p.x + R, p.y + R);

    ImGui::InvisibleButton(label, ImVec2(R * 2, R * 2 + 16));
    bool ch = false;

    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0))
    {
        *v = 0;
        ch = true;
    }
    if (RCP(label, v))
        ch = true;

    if (ImGui::IsItemActive() && io.MouseDelta.y != 0)
    {
        *v -= io.MouseDelta.y * (v_max - v_min) / 150;
        if (*v < v_min)
            *v = v_min;
        if (*v > v_max)
            *v = v_max;
        ch = true;
    }
    float t = (*v - v_min) / (v_max - v_min), a0 = (float)(M_PI * .75), a1 = (float)(M_PI * 2.25),
          av = a0 + (a1 - a0) * t;

    dl->PathArcTo(cn, R - 2, a0, a1, 40);
    dl->PathStroke(IM_COL32(60, 60, 60, 255), false, 3.5f);

    if (t > .001f)
    {
        dl->PathArcTo(cn, R - 2, a0, av, 40);
        dl->PathStroke(IM_COL32(160, 160, 160, 255), false, 3.5f);
    }
    dl->AddLine(cn, ImVec2(cn.x + cosf(av) * (R - 7), cn.y + sinf(av) * (R - 7)), IM_COL32(200, 200, 200, 200), 2);

    dl->AddCircleFilled(cn, 2.5f, IM_COL32(200, 200, 200, 255));

    char buf[32];
    snprintf(buf, 32, fmt, *v);
    ImVec2 ts = ImGui::CalcTextSize(buf);

    dl->AddText(ImVec2(cn.x - ts.x * .5f, cn.y + 4), IM_COL32(255, 255, 255, 255), buf);

    ImVec2 ls = ImGui::CalcTextSize(label);
    dl->AddText(ImVec2(cn.x - ls.x * .5f, p.y + R * 2 + 1), IM_COL32(160, 160, 160, 255), label);

    return ch;
}
bool Intellipan(const char *label, float *x, float *y, int *mode)
{
    ImGuiIO &io = ImGui::GetIO();
    ImDrawList *dl = ImGui::GetWindowDrawList();

    float sz = 130, hH = 34, tH = sz + hH;
    ImVec2 p = ImGui::GetCursorScreenPos();

    dl->AddRectFilled(p, ImVec2(p.x + sz, p.y + hH), IM_COL32(30, 30, 30, 255));

    ImVec2 t1 = ImGui::CalcTextSize("INTELLI"), t2 = ImGui::CalcTextSize("PAN");

    float tx = p.x + (sz - t1.x - t2.x) * .5f;

    dl->AddText(ImVec2(tx, p.y + 2), IM_COL32(160, 160, 160, 255), "INTELLI");

    dl->AddText(ImVec2(tx + t1.x, p.y + 2), IM_COL32(255, 255, 255, 255), "PAN");

    float sy = p.y + 18;
    ImU32 sc = IM_COL32(160, 160, 160, 255);

    if (*mode == 0)
    {
        dl->AddText(ImVec2(p.x + 4, sy), IM_COL32(255, 255, 255, 255), "VOICE");
        ImVec2 cs = ImGui::CalcTextSize("Color Panel");
        dl->AddText(ImVec2(p.x + sz - cs.x - 4, sy), sc, "Color Panel");
    }
    else if (*mode == 1)
    {
        dl->AddText(ImVec2(p.x + 4, sy), IM_COL32(255, 255, 255, 255), "MODULATION");
        ImVec2 fs = ImGui::CalcTextSize("Fx Panel");
        dl->AddText(ImVec2(p.x + sz - fs.x - 4, sy), sc, "Fx Panel");
    }
    else
    {
        dl->AddText(ImVec2(p.x + 4, sy), IM_COL32(255, 255, 255, 255), "POSITION");
        ImVec2 ds = ImGui::CalcTextSize("3D Panel");
        dl->AddText(ImVec2(p.x + sz - ds.x - 4, sy), sc, "3D Panel");
    }
    ImVec2 gA = ImVec2(p.x, p.y + hH), gB = ImVec2(p.x + sz, p.y + tH);

    float cx = gA.x + sz * .5f, cy = gA.y + sz * .5f;

    dl->AddRectFilled(gA, gB, IM_COL32(25, 25, 25, 255));
    dl->AddRect(gA, gB, IM_COL32(80, 80, 80, 255));

    ImU32 gc = IM_COL32(45, 45, 45, 255);

    if (*mode == 2)
    {
        float aR = sz * .4f, aCy = gA.y + sz * .65f;
        dl->PathArcTo(ImVec2(cx, aCy), aR, (float)(-M_PI), 0, 40);
        dl->PathStroke(IM_COL32(80, 80, 80, 255), false, 1.5f);
        dl->AddLine(ImVec2(cx, aCy - aR), ImVec2(cx, aCy), gc);
        dl->AddText(ImVec2(gA.x + 8, cy - 6), sc, "L");
        dl->AddText(ImVec2(gB.x - 16, cy - 6), sc, "R");
    }
    else
    {
        int gd = (*mode == 1) ? 4 : 2;
        for (int i = 1; i < gd; i++)
        {
            dl->AddLine(ImVec2(gA.x + (sz / gd) * i, gA.y), ImVec2(gA.x + (sz / gd) * i, gB.y), gc);
            dl->AddLine(ImVec2(gA.x, gA.y + (sz / gd) * i), ImVec2(gB.x, gA.y + (sz / gd) * i), gc);
        }
        dl->AddLine(ImVec2(cx, gA.y), ImVec2(cx, gB.y), IM_COL32(55, 55, 55, 255));
        dl->AddLine(ImVec2(gA.x, cy), ImVec2(gB.x, cy), IM_COL32(55, 55, 55, 255));
    }
    if (*mode == 0)
    {
        dl->AddText(ImVec2(gA.x + 6, gB.y - 16), sc, "Lo");
        dl->AddText(ImVec2(gB.x - 22, gB.y - 16), sc, "Hi");
        ImVec2 fs = ImGui::CalcTextSize("fx echo");
        dl->AddText(ImVec2(cx - fs.x * .5f, gA.y + sz * .3f), sc, "fx echo");
        ImVec2 bs = ImGui::CalcTextSize("brightness");
        dl->AddText(ImVec2(cx - bs.x * .5f, gA.y + sz * .3f + 14), IM_COL32(120, 120, 120, 200), "brightness");
    }
    ImGui::SetCursorScreenPos(gA);
    ImGui::InvisibleButton(label, ImVec2(sz, sz));
    bool ch = false;

    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0))
    {
        *x = 0;
        *y = 0;
        ch = true;
        s_DC = true;
    }
    if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(1))
        *mode = (*mode + 1) % 3;

    if (ImGui::IsItemActive() && !s_DC)
    {
        *x = (io.MousePos.x - cx) / (sz * .5f);
        *y = -(io.MousePos.y - cy) / (sz * .5f);
        if (*x < -1)
            *x = -1;
        if (*x > 1)
            *x = 1;
        if (*y < -1)
            *y = -1;
        if (*y > 1)
            *y = 1;
        ch = true;
    }
    if (!ImGui::IsMouseDown(0))
        s_DC = false;

    ImVec2 np;
    if (*mode == 2)
        np = ImVec2(cx + (*x * sz * .45f), cy - (*y * sz * .45f));

    else
    {
        if (fabsf(*x) < .001f && fabsf(*y) < .001f)
            np = ImVec2(cx, gB.y - 6);
        else
            np = ImVec2(cx + (*x * sz * .45f), cy - (*y * sz * .45f));
    }
    if (np.x < gA.x + 4)
        np.x = gA.x + 4;
    if (np.x > gB.x - 4)
        np.x = gB.x - 4;
    if (np.y < gA.y + 4)
        np.y = gA.y + 4;
    if (np.y > gB.y - 4)
        np.y = gB.y - 4;

    dl->AddRectFilled(ImVec2(np.x - 5, np.y - 5), ImVec2(np.x + 5, np.y + 5),
                      (*mode == 0) ? IM_COL32(229, 20, 0, 255) : IM_COL32(160, 160, 160, 255));

    ImGui::SetCursorScreenPos(ImVec2(p.x, p.y + tH + 4));
    return ch;
}
bool DrawPanBox(const char *label, float *x, float *y)
{
    ImGuiIO &io = ImGui::GetIO();
    ImDrawList *dl = ImGui::GetWindowDrawList();

    float sz = 120;
    ImVec2 p = ImGui::GetCursorScreenPos();
    float cx = p.x + sz * .5f, cy = p.y + sz * .5f;

    ImGui::InvisibleButton(label, ImVec2(sz, sz));
    bool ch = false;

    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0))
    {
        *x = 0;
        *y = 0;
        ch = true;
        s_PDC = true;
    }
    if (ImGui::IsItemActive() && !s_PDC)
    {
        *x = (io.MousePos.x - cx) / (sz * .5f);
        *y = -(io.MousePos.y - cy) / (sz * .5f);
        if (*x < -1)
            *x = -1;
        if (*x > 1)
            *x = 1;
        if (*y < -1)
            *y = -1;
        if (*y > 1)
            *y = 1;
        ch = true;
    }
    if (!ImGui::IsMouseDown(0))
        s_PDC = false;

    dl->AddRectFilled(p, ImVec2(p.x + sz, p.y + sz), IM_COL32(25, 25, 25, 255));
    dl->AddRect(p, ImVec2(p.x + sz, p.y + sz), IM_COL32(80, 80, 80, 255));

    dl->AddLine(ImVec2(cx, p.y), ImVec2(cx, p.y + sz), IM_COL32(50, 50, 50, 255));
    dl->AddLine(ImVec2(p.x, cy), ImVec2(p.x + sz, cy), IM_COL32(50, 50, 50, 255));

    ImU32 lc = IM_COL32(160, 160, 160, 255);

    ImVec2 fs = ImGui::CalcTextSize("Front");
    dl->AddText(ImGui::GetFont(), 10, ImVec2(cx - fs.x * .3f, p.y + 1), lc, "Front");

    ImVec2 rs = ImGui::CalcTextSize("Rear");
    dl->AddText(ImGui::GetFont(), 10, ImVec2(cx - rs.x * .3f, p.y + sz - 12), lc, "Rear");

    dl->AddText(ImGui::GetFont(), 10, ImVec2(p.x + 2, cy - 5), lc, "L");
    dl->AddText(ImGui::GetFont(), 10, ImVec2(p.x + sz - 10, cy - 5), lc, "R");

    dl->AddCircleFilled(ImVec2(cx + (*x * sz * .45f), cy - (*y * sz * .45f)), 4, IM_COL32(229, 20, 0, 255));

    ImGui::SetCursorScreenPos(ImVec2(p.x, p.y + sz + 3));
    return ch;
}
bool CustomGainSlider(const char *label, float *v, float v_min, float v_max, const char *sname, const char *fmt)
{
    ImGuiIO &io = ImGui::GetIO();
    ImDrawList *dl = ImGui::GetWindowDrawList();

    float trackW = 20, totalW = 50, height = 260, hR = 25;
    ImVec2 p = ImGui::GetCursorScreenPos();

    ImGui::InvisibleButton(label, ImVec2(totalW, height));
    bool ch = false;

    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0))
    {
        *v = 0;
        ch = true;
    }
    if (RCP(label, v))
        ch = true;

    if (ImGui::IsItemActive() && io.MouseDelta.y != 0 && !ImGui::IsMouseDoubleClicked(0))
    {
        *v -= io.MouseDelta.y * (v_max - v_min) / height;
        if (*v < v_min)
            *v = v_min;
        if (*v > v_max)
            *v = v_max;
        ch = true;
    }
    float t;
    if (*v >= 0)
        t = .7f + (*v / 12) * .3f;
    else
        t = .7f * (1 + *v / 60);

    float tX = p.x + (totalW - trackW) * .5f, cX = p.x + totalW * .5f;

    bool act = fabsf(*v) >= .01f;
    ImU32 fC = act ? IM_COL32(229, 20, 0, 255) : IM_COL32(130, 130, 130, 255);

    dl->AddRectFilled(ImVec2(tX, p.y), ImVec2(tX + trackW, p.y + height), IM_COL32(30, 30, 30, 255), trackW * .5f);

    dl->AddRect(ImVec2(tX, p.y), ImVec2(tX + trackW, p.y + height), IM_COL32(160, 160, 160, 100), trackW * .5f);

    float hY = p.y + height - (t * height);
    if (hY < p.y + hR)
        hY = p.y + hR;
    if (hY > p.y + height - hR)
        hY = p.y + height - hR;

    dl->AddRectFilled(ImVec2(tX, hY), ImVec2(tX + trackW, p.y + height), fC, trackW * .5f);

    dl->AddCircleFilled(ImVec2(cX, hY), hR, act ? IM_COL32(229, 20, 0, 180) : IM_COL32(130, 130, 130, 180));

    dl->AddCircle(ImVec2(cX, hY), hR, IM_COL32(180, 180, 180, 100));

    // Bold dB text
    char buf[32];
    if (fabsf(*v) < .01f)
        snprintf(buf, 32, "0dB");
    else
        snprintf(buf, 32, fmt, *v);

    ImFont *bf = g_BoldFont ? g_BoldFont : ImGui::GetFont();

    float bfs = bf->LegacySize;

    ImVec2 ts = bf->CalcTextSizeA(bfs, FLT_MAX, 0, buf);

    dl->AddText(bf, bfs, ImVec2(cX - ts.x * .5f, hY - ts.y * .5f), IM_COL32(255, 255, 255, 255), buf);

    // Rotated strip name inside track
    if (sname && bf)
    {
        ImU32 nC = act ? IM_COL32(100, 10, 0, 255) : IM_COL32(60, 60, 60, 255);

        DrawRotText(dl, bf, ImVec2(tX + (trackW - bfs) * .5f, p.y + height - 8), sname, nC);
    }
    return ch;
}
bool ToggleButton(const char *label, bool *v, bool routing)
{
    bool ch = false, on = *v;

    ImU32 bC = on ? IM_COL32(229, 20, 0, 255) : IM_COL32(100, 100, 100, 255);

    ImU32 tC = on ? IM_COL32(229, 20, 0, 255) : IM_COL32(160, 160, 160, 255);

    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(.18f, .18f, .18f, 1));

    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(.22f, .22f, .22f, 1));
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(tC));

    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3);

    ImGui::PushStyleColor(ImGuiCol_Border, ImGui::ColorConvertU32ToFloat4(bC));

    if (ImGui::Button(label, ImVec2(48, 20)))
    {
        *v = !*v;
        ch = true;
    }
    ImGui::PopStyleColor(5);
    ImGui::PopStyleVar(2);
    return ch;
}
bool KButton(const char *label, float *v)
{
    int m = (int)(*v + .5f);
    if (m < 0)
        m = 0;
    if (m > 4)
        m = 4;

    const char *lb[] = {"K", "K-m", "K-1", "K-2", "K-v"};
    bool on = (m > 0);

    ImU32 bC = on ? IM_COL32(229, 20, 0, 255) : IM_COL32(100, 100, 100, 255),
          tC = on ? IM_COL32(229, 20, 0, 255) : IM_COL32(160, 160, 160, 255);

    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(.18f, .18f, .18f, 1));

    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(.22f, .22f, .22f, 1));
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(tC));

    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3);

    ImGui::PushStyleColor(ImGuiCol_Border, ImGui::ColorConvertU32ToFloat4(bC));

    char id[64];
    snprintf(id, 64, "%s##kb", lb[m]);
    bool ch = false;

    if (ImGui::Button(id, ImVec2(48, 20)))
    {
        m = (m + 1) % 5;
        *v = (float)m;
        ch = true;
    }
    ImGui::PopStyleColor(5);
    ImGui::PopStyleVar(2);
    return ch;
}
void PeakMeter(const char *label, float pv, float height)
{
    ImDrawList *dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();

    float w = 12;
    int sg = 20;
    float sH = (height - sg + 1) / sg;

    dl->AddRectFilled(p, ImVec2(p.x + w, p.y + height), IM_COL32(20, 20, 20, 255));

    if (pv < 0)
        pv = 0;
    if (pv > 1)
        pv = 1;
    int lit = (int)(pv * sg);

    for (int i = 0; i < sg; i++)
    {
        float sy = p.y + height - (i + 1) * (sH + 1);

        dl->AddRectFilled(ImVec2(p.x + 1, sy), ImVec2(p.x + w - 1, sy + sH),
                          (i < lit) ? IM_COL32(229, 20, 0, 255) : IM_COL32(40, 40, 40, 255));
    }
    dl->AddRect(p, ImVec2(p.x + w, p.y + height), IM_COL32(60, 60, 60, 255));

    ImGui::Dummy(ImVec2(w, height));
}
} // namespace UI
