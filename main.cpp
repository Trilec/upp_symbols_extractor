/*
-------------------------------------------------------------------------------
 Material Symbols — SVG Extractor (src/ → per-category compressed headers)
-------------------------------------------------------------------------------
Objective
  - Scan a local clone of Google's *Material Icons* legacy "src" tree.
  - Collect ONLY 24px.svg for supported styles (outlined, rounded, sharp).
  - Emit ONE header per category: "icons_<category>.h".
  - Each header contains:
      enum Style { OUTLINED, ROUNDED, SHARP };
      struct BaseSVGIcon {
        const char* category;   // sanitized category (also the namespace)
        Style       style;      // OUTLINED|ROUNDED|SHARP
        const char* name;       // sanitized icon name
        const char* source;     // relative path to original 24px.svg in src/
        const char* b64zIcon;   // Base64(zlib(minified-SVG))
      };
      static const BaseSVGIcon kIcons[];
      static const int         kIconCount;

Functionality (high level)
  1) UI lets the user pick input (repo root or src/) and an output folder.
  2) Category checkboxes let the user constrain which categories are emitted.
     - "All" acts as a master toggle.
     - A category limit spinner can stop after N emitted categories (0 = no limit).
  3) Processing flow:
     - Validate structure.
     - Scan src/<category>/<icon>/<style>/24px.svg.
     - Minify + zlib compress SVG → Base64; emit per-category header with blobs + table.
     - Skip empty categories (no supported 24px.svg found).
     - Append a running, non-clearing log in the status pane.

General usage
  - Set input to your repo root or directly to ".../material-design-icons/src".
  - Set output to any writable folder.
  - Tick the categories you want (or use "All").
  - Optionally set "Limit Categories" to cap how many category headers to emit.
  - Click "Process".
  - Generated headers include a clear format signature: "FORMAT: KICONS_V1_NO_DECODERS".
  - Consumers decode with:
        String svg = ZDecompress(Base64Decode(icon.b64zIcon));

Notes
  - We intentionally do not emit per-icon decode functions; apps do a single global decode.
  - Minimal UI styling; defaults to the platform theme (white/neutral).
  - Hard-coded default paths remain for debug convenience, as requested.

-------------------------------------------------------------------------------
*/

#include <CtrlLib/CtrlLib.h>
#include <Painter/Painter.h>
#include <plugin/z/z.h>
#include <StageCard/StageCard.h>

using namespace Upp;

// ==============================
// DragBadgeButton (UI helper)
// ==============================
class DragBadgeButton : public Button {
public:
    typedef DragBadgeButton CLASSNAME;

    enum Mode   { DRAGABLE, DROPPED, NORMAL };
    enum Layout { ICON_CENTER_TEXT_BOTTOM, ICON_CENTER_TEXT_TOP, TEXT_CENTER, ICON_ONLY_CENTER };
    enum { ST_NORMAL = 0, ST_HOT = 1, ST_PRESSED = 2, ST_DISABLED = 3, ST_COUNT = 4 };

    struct Palette { Color face[ST_COUNT], border[ST_COUNT], ink[ST_COUNT]; };
    struct Payload { String flavor, id, group, name, text, badge; };

    DragBadgeButton() {
        Transparent();
        SetFrame(NullFrame());
        SetMinSize(Size(DPI(60), DPI(24)));
        SetFont(StdFont().Height(DPI(10)));
        SetBadgeFont(StdFont().Height(DPI(17)));
        SetRadius(DPI(8)).SetStroke(1).EnableDashed(false).EnableFill(true);
        SetHoverTintPercent(40);
        SetSelected(false);
        SetSelectionColors(Color(37, 99, 235), SColorHighlightText(), White());
        SetLayout(TEXT_CENTER);
        mode = NORMAL;
        SetBaseColors(Blend(SColorPaper(), SColorFace(), 20), SColorShadow(), SColorText());
    }

    // payload / content setters
    DragBadgeButton& SetPayload(const Payload& p) { payload = p; Refresh(); return *this; }
    const Payload&   GetPayload() const           { return payload; }
    DragBadgeButton& SetLabel(const String& t)    { Button::SetLabel(t); Refresh(); return *this; }
    DragBadgeButton& SetBadgeText(const String& b){ badgeText = b; Refresh(); return *this; }

    // appearance / layout
    DragBadgeButton& SetLayout(Layout l)          { layout = l; Refresh(); return *this; }
    DragBadgeButton& SetFont(Font f)              { textFont = f; Refresh(); return *this; }
    DragBadgeButton& SetBadgeFont(Font f)         { badgeFont = f; Refresh(); return *this; }
    DragBadgeButton& SetHoverColor(Color c)       { hoverColor = c; return *this; }
    DragBadgeButton& SetTileSize(Size s)          { SetMinSize(s); return *this; }
    DragBadgeButton& SetTileSize(int w, int h)    { return SetTileSize(Size(w, h)); }

    // state / selection
    DragBadgeButton& SetMode(Mode m)              { mode = m; Refresh(); return *this; }
    DragBadgeButton& SetSelected(bool on = true)  { selected = on; Refresh(); return *this; }
    bool             IsSelected() const           { return selected; }
    DragBadgeButton& SetSelectionColors(Color faceC, Color borderC, Color inkC = SColorHighlightText())
    { selFace = faceC; selBorder = borderC; selInk = inkC; return *this; }

    // palette baseline
    DragBadgeButton& SetPalette(const Palette& p) { pal = p; Refresh(); return *this; }
    Palette          GetPalette() const           { return pal; }
    DragBadgeButton& SetBaseColors(Color face, Color border, Color ink, int hot_pct = 12, int press_pct = 14) {
        pal.face[ST_NORMAL]   = face;
        pal.face[ST_HOT]      = Blend(face, White(), hot_pct);
        pal.face[ST_PRESSED]  = Blend(face, Black(), press_pct);
        pal.face[ST_DISABLED] = Blend(SColorFace(), SColorPaper(), 60);
        pal.border[ST_NORMAL] = border;
        pal.border[ST_HOT]    = Blend(border, SColorHighlight(), 20);
        pal.border[ST_PRESSED]= Blend(border, Black(), 15);
        pal.border[ST_DISABLED]= Blend(border, SColorDisabled(), 35);
        pal.ink[ST_NORMAL]    = ink;
        pal.ink[ST_HOT]       = ink;
        pal.ink[ST_PRESSED]   = ink;
        pal.ink[ST_DISABLED]  = SColorDisabled();
        Refresh();
        return *this;
    }

    // geometry / stroke
    DragBadgeButton& SetRadius(int px)  { radius = max(0, px); Refresh(); return *this; }
    DragBadgeButton& SetStroke(int th)  { stroke = max(0, th); Refresh(); return *this; }
    DragBadgeButton& EnableDashed(bool on = true) { dashed = on; Refresh(); return *this; }
    DragBadgeButton& SetDashPattern(const String& d){ dash = d; Refresh(); return *this; }
    DragBadgeButton& EnableFill(bool on = true)   { fill = on; Refresh(); return *this; }
    DragBadgeButton& SetHoverTintPercent(int pct) { hoverPct = clamp(pct, 0, 100); return *this; }

    // DnD preview (not central to this app, but harmless)
    void LeftDrag(Point, dword) override {
        if(mode != DRAGABLE || IsEmpty(payload.flavor)) return;
        Image sample = MakeDragSample();
        DoDragAndDrop(InternalClip(*this, payload.flavor), sample, DND_COPY);
    }
    void LeftDown(Point p, dword k) override {
        if(mode == DROPPED && WhenRemove) WhenRemove();
        Button::LeftDown(p, k);
    }

    // custom paint for rounded button; keeps default white-ish theme if not themed
    void Paint(Draw& w) override {
        const int st = StateIndex();
        Color faceC   = pal.face[st];
        Color borderC = pal.border[st];
        Color inkC    = pal.ink[st];

        if(HasMouse() && st == ST_HOT && !selected) {
            Color target = IsNull(hoverColor) ? White() : hoverColor;
            faceC = Blend(faceC, target, hoverPct);
        }
        if(selected) {
            if(!IsNull(selFace))   faceC = selFace;
            if(!IsNull(selBorder)) borderC = selBorder;
            if(!IsNull(selInk))    inkC = selInk;
        }

        Size sz = GetSize();
        ImageBuffer ib(sz);
        Fill(~ib, RGBAZero(), ib.GetLength());
        {
            BufferPainter p(ib, MODE_ANTIALIASED);
            const double inset = 0.5;
            const double x = inset, y = inset, wdt = sz.cx - 2 * inset, hgt = sz.cy - 2 * inset;
            p.Begin();
            if(radius > 0) p.RoundedRectangle(x, y, wdt, hgt, radius);
            else           p.Rectangle(x, y, wdt, hgt);
            if(fill)       p.Fill(faceC);
            if(stroke > 0) { if(dashed) p.Dash(dash, 0.0); p.Stroke(stroke, borderC); }
            p.End();
        }
        w.DrawImage(0, 0, ib);

        Rect r = Rect(sz).Deflated(DPI(6), DPI(4));
        const String label_txt = AsString(GetLabel());

        if(layout == ICON_CENTER_TEXT_BOTTOM) {
            bool hasBadge = !badgeText.IsEmpty();
            if(!hasBadge) {
                Size ts = GetTextSize(label_txt, textFont);
                w.DrawText(r.left + (r.GetWidth() - ts.cx) / 2,
                           r.top  + (r.GetHeight() - ts.cy) / 2,
                           label_txt, textFont, inkC);
            } else {
                Size gsz = GetTextSize(badgeText, badgeFont);
                Size ts  = GetTextSize(label_txt, textFont);
                int ty   = r.bottom - ts.cy;
                int gx   = r.left + (r.GetWidth() - gsz.cx) / 2;
                int gy   = r.top + (r.GetHeight() - (gsz.cy + DPI(4) + ts.cy)) / 2;
                w.DrawText(gx, gy, badgeText, badgeFont, inkC);
                w.DrawText(r.left + (r.GetWidth() - ts.cx) / 2, ty, label_txt, textFont, inkC);
            }
        }
        else if(layout == TEXT_CENTER) {
            Size ts = GetTextSize(label_txt, textFont);
            w.DrawText(r.left + (r.GetWidth() - ts.cx) / 2,
                       r.top  + (r.GetHeight() - ts.cy) / 2,
                       label_txt, textFont, inkC);
        }
        else { // ICON_ONLY_CENTER or ICON_CENTER_TEXT_TOP
            Size gsz = GetTextSize(badgeText, badgeFont);
            w.DrawText(r.left + (r.GetWidth() - gsz.cx) / 2,
                       r.top  + (r.GetHeight() - gsz.cy) / 2,
                       badgeText, badgeFont, inkC);
        }
    }

    Callback WhenRemove;

private:
    int  StateIndex() const {
        if(!IsShowEnabled())         return ST_DISABLED;
        if(IsPush() || HasCapture()) return ST_PRESSED;
        return HasMouse() ? ST_HOT : ST_NORMAL;
    }
    Image MakeDragSample() const {
        Size sz = GetSize();
        if(sz.cx <= 0 || sz.cy <= 0) return Image();
        ImageBuffer ib(sz);
        Fill(~ib, RGBAZero(), ib.GetLength());
        { BufferPainter p(ib); const_cast<DragBadgeButton*>(this)->Paint(p); }
        return Image(ib);
    }

    Payload    payload;
    String     badgeText;
    Layout     layout = TEXT_CENTER;
    Palette    pal;
    Font       textFont, badgeFont;
    int        radius = DPI(8), stroke = 1;
    bool       dashed = false, fill = true;
    String     dash   = "5,5";
    int        hoverPct = 22;
    bool       selected = false;
    Color      selFace = Null, selBorder = Null, selInk = Null;
    Mode       mode = NORMAL;
    Color      hoverColor = Null;
};

// ==============================
// IconExtractor (src/ 24px → per-category headers)
// ==============================

// Category list presented as checkboxes in UI
static const char* kCats[] = {
    "action","alert","av","communication","content","device","editor",
    "file","hardware","home","image","maps","navigation","notification",
    "places","search","social","toggle"
};

struct IconExtractor : TopWindow {
public:
    typedef IconExtractor CLASSNAME;

    // --- UI controls ---
    StageCard       headerCard;
    DragBadgeButton btnHelp, btnProcess, btnExit;
    Label           lblIn, lblOut, lblCatLimit;
    EditString      editIn, editOut;
    DragBadgeButton browseIn, browseOut;
    DocEdit         status;

    Array<Option>   optionCat;
    Option          all; // master toggle

    // --- scanning structures ---
    struct Style : Moveable<Style> { String key; String folder; }; // kept for parity
    Vector<Style> styles;

    // A single category bucket during scan
    struct Cat : Moveable<Cat> {
        String original;   // original category folder (e.g., "communication")
        String sanitized;  // sanitized identifier (e.g., "communication")
        VectorMap<String, VectorMap<String, String>> bucket; // style -> (iconId -> raw svg)
        VectorMap<String, String> id2orig;                   // iconId -> original folder name
    };

    // simple layout helpers for manual placing inside StageCard
    int colStart = DPI(8), colPad = DPI(4), colX = 0;
    int ColPos(int width, bool reset=false){
        if(reset) colX=colStart; int cur=colX; colX += DPI(width) + colPad; return cur;
    };

    // ---- ctor: set up UI and default paths ----
    IconExtractor() {
        Title("Material Symbols — SVG Extractor").Sizeable().Zoomable();
        SetRect(0, 0, DPI(900), DPI(520));

        // kept for possible symbols-mode reuse
        { Style s; s.key="outlined"; s.folder="materialsymbolsoutlined"; styles.Add(pick(s));
          s.key="rounded";  s.folder="materialsymbolsrounded";  styles.Add(pick(s));
          s.key="sharp";    s.folder="materialsymbolssharp";    styles.Add(pick(s)); }

        BuildUI();

        // Hard-coded defaults for debug (as requested)
        editIn  <<= "E:/apps/3rdParty/material-design-icons/src";
        editOut <<= "E:/apps/github/upp_symbols_extractor";
        AppendStatus("Ready. Choose folders and click Process.");
    }

    // ---- UI construction ----
    void BuildUI() {
        headerCard
            .SetTitle("Material Symbols — SVG Extractor")
            .SetSubTitleFont(StdFont().Height(DPI(10)))
            .SetSubTitle("Extract + compress 24px.svg from legacy src/ by category (outlined/rounded/sharp)")
            .SetHeaderAlign(StageCard::LEFT)
            .EnableCardFill(false).EnableCardFrame(false)
            .EnableHeaderFill(false)
            .SetHeaderGap(DPI(8))
            .SetHeaderInset(0, DPI(12), 0, 0)
            .ContentManual()
            .EnableContentScroll(false)
            .EnableContentFill(true).EnableContentFrame(true)
            .SetContentCornerRadius(DPI(8))
            .SetContentFrameThickness(1);
        Add(headerCard.HSizePos(DPI(8), DPI(8)).VSizePos(DPI(8), DPI(8)));

        // header buttons
        btnProcess.SetLabel("Process").SetStroke(0).SetBaseColors(SColorHighlight(), SColorShadow(), White());
        btnExit   .SetLabel("Exit").SetStroke(0).SetBaseColors(Color(235,66,33), SColorShadow(), White());
        btnHelp   .SetLabel("Help").SetStroke(0).SetBaseColors(GrayColor(100), SColorShadow(), White());

        btnProcess.WhenAction = THISBACK(DoProcess);
        btnExit   .WhenAction = THISBACK(Close);
        btnHelp   .WhenAction = THISBACK(ShowHelp);

        headerCard.AddHeader(btnExit   .RightPos( ColPos(90,true), DPI(90)).TopPos(DPI(10), DPI(28)));
        headerCard.AddHeader(btnProcess.RightPos( ColPos(90),      DPI(90)).TopPos(DPI(10), DPI(28)));
        headerCard.AddHeader(btnHelp   .RightPos( ColPos(90),      DPI(90)).TopPos(DPI(10), DPI(28)));

        // input / output selectors
        lblIn.SetText("Input (repo root or src/):");
        lblOut.SetText("Output folder:");
        browseIn .SetLabel("...").SetBaseColors(SColorHighlight(), SColorShadow(), White());
        browseOut.SetLabel("...").SetBaseColors(SColorHighlight(), SColorShadow(), White());

        headerCard.AddFixed(lblIn  .LeftPos( ColPos(180,true), DPI(170)).TopPos(DPI(15),  DPI(24)));
        headerCard.AddFixed(editIn .LeftPos( ColPos(520),      DPI(520)).TopPos(DPI(15),  DPI(24)));
        headerCard.AddFixed(browseIn.LeftPos(ColPos(40),       DPI(40)) .TopPos(DPI(15),  DPI(24)));

        headerCard.AddFixed(lblOut .LeftPos( ColPos(180,true), DPI(170)).TopPos(DPI(47), DPI(24)));
        headerCard.AddFixed(editOut.LeftPos( ColPos(520),      DPI(520)).TopPos(DPI(47), DPI(24)));
        headerCard.AddFixed(browseOut.LeftPos(ColPos(40),      DPI(40)) .TopPos(DPI(47), DPI(24)));

        // category filters + limit
        lblCatLimit.SetText("Limit Categories");
        headerCard.AddFixed(lblCatLimit.LeftPos( ColPos(180,true), DPI(170)).TopPos(DPI(85), DPI(24)));

        all.SetLabel("All"); all.Set(1);
        all.WhenAction = [=] {
            bool v = all.Get();
            for (Option& o : optionCat) o.Set(v);
        };
  
        int y = 85;
        headerCard.AddFixed(all.LeftPos(ColPos(80), DPI(80)).TopPos(DPI(y), DPI(22)));

        for (const char* c : kCats) {
            Option& o = optionCat.Add();
            o.Set(1);
            headerCard.AddFixed(o.SetLabel(c).LeftPos( ColPos(80), DPI(80)).TopPos(DPI(y), DPI(22)) );
            if(colX > 800) { ColPos(180,true); y += 26; }
        }

        // status log (non-clearing)
        status.SetReadOnly();
        status.SetFrame(NullFrame());
        status.SetFont(Monospace(10));
        status.Transparent(false);
        headerCard.AddFixed(status.HSizePos(DPI(10), DPI(10)).BottomPos(DPI(10), DPI(220)));

        // folder pickers (note: defaults are already set for debug)
        browseIn << [=]{
            FileSel fs; fs.ActiveDir(~editIn);
            if(fs.ExecuteSelectDir()) { editIn <<= ~fs; WarnIfStructureUnexpected(~editIn); }
        };
        browseOut << [=]{
            FileSel fs; fs.ActiveDir(~editOut);
            if(fs.ExecuteSelectDir()) editOut <<= ~fs;
        };
    }

    // ---- string sanitization for C++ identifiers ----
    static String SanitizeId(String s) {
        for(int i=0;i<s.GetCount();++i) {
            char c = s[i];
            if(!(IsAlNum(c) || c=='_')) s.Set(i, '_');
        }
        if(!s.IsEmpty() && IsDigit(s[0])) s = "_" + s;
        return ToLower(s);
    }

    // ---- map style folder name → short key we use in tables ----
    static String MapSrcStyleToKey(const String& style_dir_name) {
        const String n = ToLower(style_dir_name);
        if(n == "materialiconsoutlined") return "outlined";
        if(n == "materialiconsround")    return "rounded";
        if(n == "materialiconssharp")    return "sharp";
        return String(); // ignore baseline materialicons / others
    }
    // ---- reverse: short key → src dir name ----
    static String StyleKeyToSrcDir(const String& key) {
        if(key == "outlined") return "materialiconsoutlined";
        if(key == "rounded")  return "materialiconsround";
        if(key == "sharp")    return "materialiconssharp";
        return String();
    }

    // ---- lightweight SVG minifier (whitespace trimming near tags) ----
    static String MinifySvg(const String& in) {
        String out; out.Reserve(in.GetCount());
        bool in_space = false;
        for(int i = 0; i < in.GetCount(); ++i) {
            byte c = in[i];
            if(c == '\r' || c == '\n' || c == '\t') c = ' ';
            if(c == ' ') { if(in_space) continue; in_space = true; }
            else in_space = false;
            out.Cat(c);
        }
        String out2; out2.Reserve(out.GetCount());
        for(int i = 0; i < out.GetCount(); ++i) {
            byte c = out[i];
            if((c == ' ' || c == '\t') && i+1 < out.GetCount() && (out[i+1] == '>' || out[i+1] == '<'))
                continue;
            if((c == '<' || c == '>') && i+1 < out.GetCount() && (out[i+1] == ' ' || out[i+1] == '\t')) {
                out2.Cat(c); ++i; continue;
            }
            out2.Cat(c);
        }
        return TrimBoth(out2);
    }

    // ---- quick structure probe: do we find *any* 24px.svg in a supported style? ----
    static bool HasAtLeastOne24pxSvgSrc(const String& srcdir) {
        if(!DirectoryExists(srcdir)) return false;
        FindFile cat_ff(AppendFileName(srcdir, "*"));
        while(cat_ff) {
            if(cat_ff.IsDirectory()) {
                FindFile icon_ff(AppendFileName(AppendFileName(srcdir, cat_ff.GetName()), "*"));
                while(icon_ff) {
                    if(icon_ff.IsDirectory()) {
                        FindFile style_ff(AppendFileName(AppendFileName(AppendFileName(srcdir, cat_ff.GetName()),
                                                                        icon_ff.GetName()), "*"));
                        while(style_ff) {
                            if(style_ff.IsDirectory()) {
                                if(!MapSrcStyleToKey(style_ff.GetName()).IsEmpty()) {
                                    const String f = AppendFileName(
                                                        AppendFileName(
                                                            AppendFileName(
                                                                AppendFileName(srcdir, cat_ff.GetName()),
                                                                icon_ff.GetName()),
                                                            style_ff.GetName()),
                                                        "24px.svg");
                                    if(FileExists(f)) return true;
                                }
                            }
                            style_ff.Next();
                        }
                    }
                    icon_ff.Next();
                }
            }
            cat_ff.Next();
        }
        return false;
    }

    // ---- write one category header with blobs + kIcons table ----
    static bool WriteCategoryHeader(const String& outdir,
                                    const String& cat_san,
                                    const String& cat_orig,
                                    const VectorMap<String, VectorMap<String, String>>& bucket,
                                    const VectorMap<String, String>& id2orig)
    {
        // count icons; skip empty categories entirely
        int icon_total = 0;
        for(int si=0; si<bucket.GetCount(); ++si)
            icon_total += bucket[si].GetCount();
        if(icon_total == 0)
            return true; // nothing to emit

        const String fn       = AppendFileName(outdir, "icons_" + cat_san + ".h");
        const String tmp_fn   = fn + ".tmp";
        const String GUARD    = "UPP_MATERIAL_SYMBOLS_ICONS_" + ToUpper(cat_san) + "_H";
        const String SIGN     = "FORMAT: KICONS_V1_NO_DECODERS";

        String h;
        h << "// Auto-generated by Material Symbols SVG Extractor (v0.9)\n"
          << "// " << SIGN << "\n"
          << "// Category: " << cat_orig << "\n"
          << "// Location: E:/apps/3rdParty/material-design-icons/src/" << cat_orig << "/\n"
          << "// Each icon is Base64(zlib(minified-SVG)). Decode with ZDecompress(Base64Decode(...)).\n\n"
          << "#ifndef " << GUARD << "\n"
          << "#define " << GUARD << "\n\n"
          << "#include <Core/Core.h>\n"
          << "#include <plugin/z/z.h>\n\n"
          << "namespace " << cat_san << " {\n\n"
          << "using namespace Upp;\n\n"
          << "enum Style : uint8 { OUTLINED = 0, ROUNDED = 1, SHARP = 2 };\n\n"
          << "struct BaseSVGIcon {\n"
          << "    const char* category;\n"
          << "    Style       style;\n"
          << "    const char* name;\n"
          << "    const char* source;\n"
          << "    const char* b64zIcon;\n"
          << "};\n\n";

        auto styleEnum = [](const String& k)->String {
            if(k == "outlined") return "OUTLINED";
            if(k == "rounded")  return "ROUNDED";
            return "SHARP";
        };

        // stable style order
        Vector<String> styles;
        for(int si=0; si<bucket.GetCount(); ++si) styles.Add(bucket.GetKey(si));
        Sort(styles);

        // emit blobs
        for(const String& st_key : styles) {
            int si = bucket.Find(st_key);
            Vector<String> icon_ids;
            for(int ii=0; ii<bucket[si].GetCount(); ++ii) icon_ids.Add(bucket[si].GetKey(ii));
            Sort(icon_ids);

            for(const String& id : icon_ids) {
                const String& svg     = bucket[si].Get(id);
                const String  z       = ZCompress(MinifySvg(svg));
                const String  b64     = Base64Encode(z);
                const String  icon_orig = id2orig.Get(id, id);
                const String  src_style = StyleKeyToSrcDir(st_key);
                const String  blob_sym  = cat_san + "_" + st_key + "_" + id + "_b64z";

                h << "// " << id << " [" << st_key << "]  source: "
                  << "src/" << cat_orig << "/" << icon_orig << "/" << src_style << "/24px.svg\n";
                h << "static const char " << blob_sym << "[] =\n    \"";
                for(int i=0;i<b64.GetCount();++i) {
                    h.Cat(b64[i]);
                    if((i+1) % 80 == 0 && i+1 < b64.GetCount()) h << "\"\n    \"";
                }
                h << "\";\n\n";
            }
        }

        // emit kIcons[]
        h << "static const BaseSVGIcon kIcons[] = {\n";
        for(const String& st_key : styles) {
            int si = bucket.Find(st_key);
            Vector<String> icon_ids;
            for(int ii=0; ii<bucket[si].GetCount(); ++ii) icon_ids.Add(bucket[si].GetKey(ii));
            Sort(icon_ids);

            for(const String& id : icon_ids) {
                const String icon_orig = id2orig.Get(id, id);
                const String src_style = StyleKeyToSrcDir(st_key);
                const String blob_sym  = cat_san + "_" + st_key + "_" + id + "_b64z";
                const String source_rel = "src/" + cat_orig + "/" + icon_orig + "/" + src_style + "/24px.svg";

                h << "    { \"" << cat_san << "\", " << styleEnum(st_key) << ", \"" << id << "\", "
                  << "\"" << source_rel << "\", " << blob_sym << " },\n";
            }
        }
        h << "};\n\n"
          << "static const int kIconCount = (int)(sizeof(kIcons) / sizeof(kIcons[0]));\n\n"
          << "} // namespace " << cat_san << "\n\n"
          << "#endif // " << GUARD << "\n";

        if(!SaveFile(tmp_fn, h)) return false;

        // verify the signature we just wrote
        {
            String chk = LoadFile(tmp_fn);
            if(chk.Find("FORMAT: KICONS_V1_NO_DECODERS") < 0)
                return false;
        }

        if(FileExists(fn)) FileDelete(fn);
        if(!FileMove(tmp_fn, fn)) return false;

        return true;
    }

    // ---- status log helpers (never auto-clear) ----
    void SetStatus(const String& t) { AppendStatus(t); }
    void ClearStatus() { status.Set(WString()); }
    void AppendStatus(const String& t) {
        String cur = status.Get();
        if(!cur.IsEmpty()) cur.Cat('\n');
        cur.Cat(String(t));
        status.Set(cur);
        status.SetCursor(status.GetLength()); // keep scrolled to bottom
    }

    // ---- help dialog ----
    void ShowHelp() {
        String t;
        t
        << "Material Symbols SVG Extractor — v0.9 (src mode)\n\n"
        << "• Scans 'src/' for 24px.svg in styles: outlined / rounded / sharp.\n"
        << "• Emits one header per category: icons_<category>.h\n"
        << "• Header contains:\n"
        << "    enum Style { OUTLINED, ROUNDED, SHARP };\n"
        << "    struct BaseSVGIcon { const char* category; Style style; const char* name; const char* source; const char* b64zIcon; };\n"
        << "    static const BaseSVGIcon kIcons[];  static const int kIconCount;\n"
        << "• Decode on demand in your app:\n"
        << "    String svg = ZDecompress(Base64Decode(kIcons[i].b64zIcon));\n\n"
        << "Expected layout:\n"
        << "  src/<category>/<icon>/<style>/24px.svg\n";
        PromptOK(DeQtf(t));
    }

    // ---- guard: ensure the selected folder is repo root or src/ and has content ----
    bool WarnIfStructureUnexpected(const String& picked) {
        String src_dir = DirectoryExists(AppendFileName(picked, "src"))
                       ? AppendFileName(picked, "src")
                       : (GetFileName(picked) == "src" ? picked : String());
        if(IsNull(src_dir) || !DirectoryExists(src_dir)) {
            String m;
            m << "Please select the repository root or the 'src' folder.\n\n"
              << "Expected layout:\n"
              << "  <repo_root>/src/<category>/<icon>/<style>/24px.svg\n\n"
              << "Selected path:\n  " << picked;
            PromptOK(DeQtf(m));
            return true;
        }
        if(!HasAtLeastOne24pxSvgSrc(src_dir)) {
            String m;
            m << "No '24px.svg' files were found under:\n  " << src_dir << "\n\n"
              << "Expected layout:\n"
              << "  src/<category>/<icon>/<style>/24px.svg\n"
              << "Styles: materialiconsoutlined, materialiconsround, materialiconssharp.";
            PromptOK(DeQtf(m));
            return true;
        }
        return false;
    }


    // Build a lowercase set of enabled categories from the checkboxes.
    // If all are checked, returns an *empty* set, meaning "no filtering".
    Index<String> BuildEnabledSetLower() const {
        Index<String> out;
        int checked = 0;
        for(int i=0; i<optionCat.GetCount(); ++i) {
            const bool on = optionCat[i].Get();
            if(on) { out.Add(ToLower(String(kCats[i]))); ++checked; }
        }
        if(checked == optionCat.GetCount()) {
            out.Clear(); // no filtering
        }
        return out;
    }

    // Decide if a category is allowed, given the enabled set (or no filtering).
    static bool IsAllowedCategory(const String& cat_orig, const String& cat_san, const Index<String>& enabled) {
        if(enabled.IsEmpty()) return true; // no filtering
        String lo = ToLower(cat_orig);
        String ls = ToLower(cat_san);
        return enabled.Find(lo) >= 0 || enabled.Find(ls) >= 0;
    }



     void DoProcess() {
        auto log = [&](const String& s){ AppendStatus(s); Ctrl::ProcessEvents(); };

        log("Locating directories ...");

        // Use editors (debug defaults are pre-filled in ctor)
        String picked = ~editIn;
        String outdir = ~editOut;

        if(IsEmpty(picked) || IsEmpty(outdir)) { log("Error: select both input and output directories."); return; }
        if(!DirectoryExists(picked))           { log("Error: input directory does not exist."); return; }
        if(!DirectoryExists(outdir) && !RealizeDirectory(outdir)) {
            log("Error: output directory cannot be created."); return;
        }

        String src_dir = DirectoryExists(AppendFileName(picked, "src"))
                       ? AppendFileName(picked, "src")
                       : (GetFileName(picked) == "src" ? picked : String());
        if(IsNull(src_dir)) {
            log("Error: please point to the repository root or the 'src' folder.");
            PromptOK("Extractor targets 'src/' layout:\n\n  src/<category>/<icon>/<style>/24px.svg");
            return;
        }
        if(!HasAtLeastOne24pxSvgSrc(src_dir)) {
            log("No 24px.svg files found under src/.");
            PromptOK("Did not find any 24px.svg in:\n  " + src_dir);
            return;
        }

        // Build enabled category set (lowercase); empty set => no filtering
        Index<String> enabledCats = BuildEnabledSetLower();
        if(!enabledCats.GetCount()) {
            log("Filter: all categories enabled.");
        } else {
            String list;
            for(int i=0;i<enabledCats.GetCount();++i) {
                if(i) list << ", ";
                list << enabledCats[i];
            }
            log("Filter: enabled categories = [" + list + "]");
        }

        log("Scanning (early-filtering categories) ...");

        int cats_seen = 0, cats_skipped_filtered = 0, cats_emitted = 0;
        int icons_seen = 0, files_found = 0;
        int empty_selected_skipped = 0;

        // Iterate categories
        FindFile cat_ff(AppendFileName(src_dir, "*"));
        while(cat_ff) {
            if(cat_ff.IsDirectory()) {
                const String cat_orig = cat_ff.GetName();
                const String cat_san  = SanitizeId(cat_orig);
                const String cat_path = AppendFileName(src_dir, cat_orig);
                ++cats_seen;

                // EARLY FILTER: if not enabled, skip without descending
                if(!IsAllowedCategory(cat_orig, cat_san, enabledCats)) {
                    log(Format(" category: %s  (%s)  → skipped (filtered)", cat_orig, cat_path));
                    ++cats_skipped_filtered;
                    cat_ff.Next();
                    continue;
                }

                log(Format(" category: %s  (%s)  → processing ...", cat_orig, cat_path));

                // Build category bucket
                Cat c; c.original = cat_orig; c.sanitized = cat_san;

                FindFile icon_ff(AppendFileName(cat_path, "*"));
                while(icon_ff) {
                    if(icon_ff.IsDirectory()) {
                        const String icon_orig = icon_ff.GetName();
                        const String icon_id   = SanitizeId(icon_orig);
                        const String icon_path = AppendFileName(cat_path, icon_orig);
                        ++icons_seen;

                        c.id2orig.GetAdd(icon_id) = icon_orig;

                        FindFile style_ff(AppendFileName(icon_path, "*"));
                        while(style_ff) {
                            if(style_ff.IsDirectory()) {
                                const String st_dir  = style_ff.GetName();
                                const String st_key  = MapSrcStyleToKey(st_dir);
                                if(IsNull(st_key)) { style_ff.Next(); continue; }

                                const String svg_24 = AppendFileName(AppendFileName(icon_path, st_dir), "24px.svg");
                                if(FileExists(svg_24)) {
                                    const String svg = LoadFile(svg_24);
                                    if(!svg.IsEmpty()) {
                                        int si = c.bucket.FindAdd(st_key);
                                        c.bucket[si].GetAdd(icon_id) = svg;
                                        ++files_found;
                                    } else {
                                        log(Format("   WARN: failed to read %s", svg_24));
                                    }
                                } else {
                                    log(Format("   WARN: missing %s", svg_24));
                                }
                            }
                            style_ff.Next();
                        }
                    }
                    icon_ff.Next();
                }

                // Count icons in this category
                int cnt = 0; for(int s=0; s<c.bucket.GetCount(); ++s) cnt += c.bucket[s].GetCount();
                if(cnt == 0) {
                    ++empty_selected_skipped;
                    log(Format("   note: category '%s' is empty → skipped", cat_orig));
                } else {
                    // Emit immediately
                    const bool ok = WriteCategoryHeader(outdir, c.sanitized, c.original, c.bucket, c.id2orig);
                    const String out_path = AppendFileName(outdir, "icons_" + c.sanitized + ".h");
                    if(ok) { ++cats_emitted; log(Format("   DONE  [to file %s]", out_path)); }
                    else   { log(Format("   ERROR [failed to write %s]", out_path)); }
                }
            }
            cat_ff.Next();
        }

        // Summary
        log("");
        log(Format("Summary: scanned %d categories, emitted %d, skipped filtered %d, skipped empty (selected) %d",
                   cats_seen, cats_emitted, cats_skipped_filtered, empty_selected_skipped));
        log(Format("         icons seen (folders): %d, 24px files loaded: %d", icons_seen, files_found));
        log(Format("Output:  %s", outdir));
        log("DONE");
    }
};
// ------------------------------
// App entry
// ------------------------------
GUI_APP_MAIN
{
    IconExtractor().Run();
}
