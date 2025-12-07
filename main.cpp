/*
-------------------------------------------------------------------------------
 Material Symbols — SVG Extractor (symbols/web → compressed headers)
-------------------------------------------------------------------------------
Version
  - v0.30.0  (Material Symbols, web-mode, per-JSON-category headers)

Purpose
  - Scan a local clone of Google's *Material Symbols* `symbols/web` tree.
  - Collect ONLY `<icon>_24px.svg` for supported styles:
        • materialsymbolsoutlined
        • materialsymbolsrounded
        • materialsymbolssharp
  - Drive filtering via an external JSON metadata file:
        • metadata style:
              { "icons": [ { "name": "...", "categories": ["alert", ...] }, ... ] }
          or arrays/maps under "icons"/"glyphs"/"symbols"/"items".
        • compact update/current_versions.json style:
              { "alert::add_alert": 17, "symbols::zoom_out_map": 329, ... }
          where the prefix before "::" is treated as the category.
  - Emit ONE header per logical JSON category:
        icons_<category>.h   e.g.  icons_alert.h, icons_content.h, ...

        namespace <category> {
            enum Style : uint8 { OUTLINED = 0, ROUNDED = 1, SHARP = 2 };

            struct BaseSVGIcon {
                const char* category;   // logical category name (e.g. "alert")
                Style       style;      // OUTLINED | ROUNDED | SHARP
                const char* name;       // sanitized icon name (snake_case)
                const char* source;     // symbols/web/<icon>/<style>/<icon>_24px.svg
                const char* b64zIcon;   // Base64(zlib(minified-SVG))
            };

            static const BaseSVGIcon kIcons[];
            static const int         kIconCount;
        }

High-level functionality
  1) UI lets the user pick:
       - Input repo root (or symbols/web),
       - Output folder,
       - Input JSON (required).
  2) Categories in the UI are ALWAYS derived from the Input JSON:
       - A summary label shows: "<file> — X categories, Y items".
       - Each checkbox shows "category (count)" where count is how many JSON
         entries reference that category.
       - "All" acts as a master toggle.
  3) Processing flow (web-mode):
       - Resolve and validate the symbols/web directory:
             symbols/web/<icon>/<style>/<icon>_24px.svg
       - Require Input JSON and load categories + per-category icon IDs.
       - Build the set of enabled categories from the checkboxes.
       - Compute the union of icon IDs needed by the selected categories.
       - For each needed icon ID:
             • Probe symbols/web/<icon_id>/<style>/ for 24px.svg in
               outlined / rounded / sharp style folders.
             • Load and cache the SVGs once per icon ID.
             • Attach the SVGs into each selected JSON category that
               references that icon ID.
       - For each non-empty category bucket:
             • Optionally apply the global "Limit Icons" cap.
             • Emit icons_<category>.h with Base64(zlib(minified-SVG)) blobs.
       - Append a running, non-clearing log in the status pane.
  4) Current limits / notes:
       - Only Material Symbols under `symbols/web` are scanned.
       - Legacy Material Icons under `src/`, `png/`, `font/`, etc. are ignored.
       - Icons that appear in JSON but have no corresponding `symbols/web`
         folder (e.g. some legacy-only glyphs) are reported as WARN lines
         but otherwise skipped.
       - A synthetic "misc" category can be added for icons present on disk
         but missing from JSON (optional).

General usage
  - Set **Input** to your repo root (e.g. "E:/apps/3rdParty/material-design-icons")
    or directly to ".../material-design-icons/symbols/web".
  - Set **Input JSON** to either:
        • material_symbols_metadata.json, or
        • update/current_versions.json, or
        • metadata.json with an "icons" array (or similar).
  - Set **Output** to any writable folder.
  - Tick the categories you care about (e.g. "alert", "content", ...).
  - Optionally set:
        • "Limit Categories" to stop after N non-empty categories.
        • "Limit Icons" to cap the total number of unique icon IDs emitted.
  - Click "Process".
  - Generated headers include a clear format signature:
        "FORMAT: KICONS_V1_NO_DECODERS".
  - Consumers decode with:
        String svg = ZDecompress(Base64Decode(icon.b64zIcon));

Optional helpers
  - "Check" button:
        • Scans symbols/web/<icon>/ folders.
        • Compares folder names (sanitized) to icon IDs from the currently
          loaded Input JSON.
        • Reports:
              - Icons present on disk but not referenced in JSON.
              - Icons referenced in JSON but missing a symbols/web folder.
  - "Check Meta" button:
        • Locates material_symbols_metadata.json next to the 'symbols' tree.
        • Compares those IDs to symbols/web/<icon>/ folders.
        • Same style of report as "Check", but always against the canonical
          Material Symbols metadata file.

Change log
  - v0.30.0
      • Switched to per-category headers (icons_<category>.h) driven
        by JSON metadata.
      • Unified support for both metadata-style JSON and compact
        current_versions.json.
      • Added global "Limit Categories" and "Limit Icons" controls.
      • Added warnings for JSON icons that do not exist under symbols/web.
      • Added optional coverage check helper (see "Check" and "Check Meta").
  - v0.20.0
      • Initial web-mode extractor:
            - Scanned symbols/web/<icon>/<style>/<icon>_24px.svg only.
            - Emitted a single icons_symbols.h header.
            - Ignored legacy Material Icons under src/.
  - v0.10.0
      • Switched to "Input JSON (required)" — no built-in category list.

-------------------------------------------------------------------------------
*/

#include <CtrlLib/CtrlLib.h>
#include <Painter/Painter.h>
#include <plugin/z/z.h>
#include <StageCard/StageCard.h>

using namespace Upp;

// Application metadata (kept here for single-source-of-truth in UI + headers)
static const char kAppVersion[]      = "0.30.0";
static const char kFormatSignature[] = "FORMAT: KICONS_V1_NO_DECODERS";

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
        SetSelectionColors(Color(37, 99, 235), SColorHighlightText(), White);
        SetLayout(TEXT_CENTER);
        mode = NORMAL;
        SetBaseColors(Blend(SColorPaper(), SColorFace(), 20), SColorShadow(), SColorText());
    }

    // payload / content setters
    DragBadgeButton& SetPayload(const Payload& p)      { payload = p; Refresh(); return *this; }
    const Payload&   GetPayload() const                { return payload; }
    DragBadgeButton& SetLabel(const String& t)         { Button::SetLabel(t); Refresh(); return *this; }
    DragBadgeButton& SetBadgeText(const String& b)     { badgeText = b; Refresh(); return *this; }

    // appearance / layout
    DragBadgeButton& SetLayout(Layout l)               { layout = l; Refresh(); return *this; }
    DragBadgeButton& SetFont(Font f)                   { textFont = f; Refresh(); return *this; }
    DragBadgeButton& SetBadgeFont(Font f)              { badgeFont = f; Refresh(); return *this; }
    DragBadgeButton& SetHoverColor(Color c)            { hoverColor = c; return *this; }
    DragBadgeButton& SetTileSize(Size s)               { SetMinSize(s); return *this; }
    DragBadgeButton& SetTileSize(int w, int h)         { return SetTileSize(Size(w, h)); }

    // state / selection
    DragBadgeButton& SetMode(Mode m)                   { mode = m; Refresh(); return *this; }
    DragBadgeButton& SetSelected(bool on = true)       { selected = on; Refresh(); return *this; }
    bool             IsSelected() const                { return selected; }
    DragBadgeButton& SetSelectionColors(Color faceC, Color borderC, Color inkC = SColorHighlightText())
    { selFace = faceC; selBorder = borderC; selInk = inkC; return *this; }

    // palette baseline
    DragBadgeButton& SetPalette(const Palette& p)      { pal = p; Refresh(); return *this; }
    Palette          GetPalette() const                { return pal; }
    DragBadgeButton& SetBaseColors(Color face, Color border, Color ink, int hot_pct = 12, int press_pct = 14) {
        pal.face[ST_NORMAL]    = face;
        pal.face[ST_HOT]       = Blend(face, White(), hot_pct);
        pal.face[ST_PRESSED]   = Blend(face, Black(), press_pct);
        pal.face[ST_DISABLED]  = Blend(SColorFace(), SColorPaper(), 60);
        pal.border[ST_NORMAL]  = border;
        pal.border[ST_HOT]     = Blend(border, SColorHighlight(), 20);
        pal.border[ST_PRESSED] = Blend(border, Black(), 15);
        pal.border[ST_DISABLED]= Blend(border, SColorDisabled(), 35);
        pal.ink[ST_NORMAL]     = ink;
        pal.ink[ST_HOT]        = ink;
        pal.ink[ST_PRESSED]    = ink;
        pal.ink[ST_DISABLED]   = SColorDisabled();
        Refresh();
        return *this;
    }

    // geometry / stroke
    DragBadgeButton& SetRadius(int px)                 { radius = max(0, px); Refresh(); return *this; }
    DragBadgeButton& SetStroke(int th)                 { stroke = max(0, th); Refresh(); return *this; }
    DragBadgeButton& EnableDashed(bool on = true)      { dashed = on; Refresh(); return *this; }
    DragBadgeButton& SetDashPattern(const String& d)   { dash = d; Refresh(); return *this; }
    DragBadgeButton& EnableFill(bool on = true)        { fill = on; Refresh(); return *this; }
    DragBadgeButton& SetHoverTintPercent(int pct)      { hoverPct = clamp(pct, 0, 100); return *this; }

    // DnD preview
    void LeftDrag(Point, dword) override {
        if(mode != DRAGABLE || IsEmpty(payload.flavor)) return;
        Image sample = MakeDragSample();
        DoDragAndDrop(InternalClip(*this, payload.flavor), sample, DND_COPY);
    }

    void LeftDown(Point p, dword k) override {
        if(mode == DROPPED && WhenRemove)
            WhenRemove();
        Button::LeftDown(p, k);
    }

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
            if(!IsNull(selFace))   faceC   = selFace;
            if(!IsNull(selBorder)) borderC = selBorder;
            if(!IsNull(selInk))    inkC    = selInk;
        }

        Size sz = GetSize();
        ImageBuffer ib(sz);
        Fill(~ib, RGBAZero(), ib.GetLength());
        {
            BufferPainter p(ib, MODE_ANTIALIASED);
            const double inset = 0.5;
            const double x = inset, y = inset, wdt = sz.cx - 2 * inset, hgt = sz.cy - 2 * inset;
            p.Begin();
            if(radius > 0)
                p.RoundedRectangle(x, y, wdt, hgt, radius);
            else
                p.Rectangle(x, y, wdt, hgt);
            if(fill)
                p.Fill(faceC);
            if(stroke > 0) {
                if(dashed)
                    p.Dash(dash, 0.0);
                p.Stroke(stroke, borderC);
            }
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
        else {
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
        if(sz.cx <= 0 || sz.cy <= 0)
            return Image();
        ImageBuffer ib(sz);
        Fill(~ib, RGBAZero(), ib.GetLength());
        {
            BufferPainter p(ib);
            const_cast<DragBadgeButton*>(this)->Paint(p);
        }
        return Image(ib);
    }

    Payload    payload;
    String     badgeText;
    Layout     layout = TEXT_CENTER;
    Palette    pal;
    Font       textFont, badgeFont;
    int        radius   = DPI(8);
    int        stroke   = 1;
    bool       dashed   = false;
    bool       fill     = true;
    String     dash     = "5,5";
    int        hoverPct = 22;
    bool       selected = false;
    Color      selFace  = Null;
    Color      selBorder= Null;
    Color      selInk   = Null;
    Mode       mode     = NORMAL;
    Color      hoverColor = Null;
};

// ==============================
// IconExtractor (symbols/web 24px → headers)
// ==============================

struct IconExtractor : TopWindow {
public:
    typedef IconExtractor CLASSNAME;

    // --- UI controls ---
    StageCard       headerCard;
    DragBadgeButton btnHelp, btnProcess, btnExit;
    DragBadgeButton btnCheckCoverage;
    DragBadgeButton btnCheckMetaCoverage;
    
    Label           lblIn, lblOut, lblMeta, lblCatLimit, lblIconLimit;
    Label           lblCatSummary;
    EditString      editIn, editOut, editMeta;
    DragBadgeButton browseIn, browseOut, browseMeta;
    DragBadgeButton btnLoadCats;
    DocEdit         status;

    Array<Option>   optionCat;
    Option          all;
    EditIntSpin     spinCatLimit;
    EditIntSpin     spinIconLimit;

    // Dynamic categories (from JSON)
    Vector<String>  categories;
    Vector<int>     catItemCounts;
    
    Vector< Index<String> > catIconIds;     // per-category icon IDs (sanitized)  
    Index<String>           allJsonIconIds; // union of all icon IDs in JSON
    Index<String>           allDiskIconIds; // union of all icon IDs present on disk (symbols/web)
    
    String          lastMetaPath;
    int             lastJsonItemCount = 0;
    int             optionsY = 0;

    // A single category bucket during scan
    struct Cat : Moveable<Cat> {
        String original;   // logical category label ("symbols")
        String sanitized;  // sanitized identifier ("symbols")
        // styleKey -> (iconId -> raw svg)
        VectorMap<String, VectorMap<String, String>> bucket;
        // iconId -> original folder name
        VectorMap<String, String> id2orig;
    };

    int colStart = DPI(8), colPad = DPI(4), colX = 0;
    int ColPos(int width, bool reset=false){
        if(reset)
            colX = colStart;
        int cur = colX;
        colX += DPI(width) + colPad;
        return cur;
    };

    IconExtractor() {
        Title("Material Symbols — SVG Extractor").Sizeable().Zoomable();
        SetRect(0, 0, DPI(900), DPI(520));

        BuildUI();

        // Hard-coded defaults for debug
        editIn  <<= "E:/apps/3rdParty/material-design-icons";
        editOut <<= "E:/apps/github/upp_symbols_extractor";

        // Default Input JSON: prefer the big material_symbols_metadata.json at repo root
        String base = (String)~editIn;
        
        // Normalize base to repo root if user points at symbols/ or symbols/web
        String repo_root = base;
        String last   = GetFileName(base);
        String parent = GetFileName(GetFileFolder(base));
        if(last == "web" && parent == "symbols")
            repo_root = GetFileFolder(GetFileFolder(base)); // .../material-design-icons
        else if(last == "symbols")
            repo_root = GetFileFolder(base);                // parent of .../symbols
        
        String guess_meta_symbols = AppendFileName(repo_root, "material_symbols_metadata.json");
        String guess_update       = AppendFileName(AppendFileName(repo_root, "update"), "current_versions.json");
        String guess_root         = AppendFileName(repo_root, "metadata.json");
        String guess_src          = AppendFileName(AppendFileName(repo_root, "src"), "metadata.json");
        
        String meta_guess;
        if(FileExists(guess_meta_symbols))
            meta_guess = guess_meta_symbols;
        else if(FileExists(guess_update))
            meta_guess = guess_update;
        else if(FileExists(guess_root))
            meta_guess = guess_root;
        else if(FileExists(guess_src))
            meta_guess = guess_src;
        else
            meta_guess = guess_meta_symbols; // last resort – user will fix path manually
        
        editMeta <<= meta_guess;

        if(FileExists(meta_guess)) {
            LoadCategoriesFromJson(meta_guess);
        } else {
            AppendStatus("Note: default Input JSON not found at:\n  " + meta_guess +
                         "\nSet Input JSON and click 'Load cats'.");
            lblCatSummary.SetText("No Input JSON loaded.");
            lblCatSummary.SetInk(SColorDisabled());
        }

        AppendStatus("Ready. Set Input repo, Input JSON (if no categories loaded), output folder, then click Process.");
    }
    
    void DoCheckCoverage() {
        auto log = [&](const String& s) {
            AppendStatus(s);
            Ctrl::ProcessEvents();
        };
    
        log("");
        log(Format("=== Coverage check (v%s) ===", kAppVersion));
    
        const String picked   = ~editIn;
        const String metaPath = ~editMeta;
    
        if(IsEmpty(picked) || !DirectoryExists(picked)) {
            log("Error: input repo is not set or does not exist.");
            PromptOK("Please select a valid Input repo root (or symbols/web) first.");
            return;
        }
    
        String webdir = ResolveSymbolsWebDir(picked);
        if(IsNull(webdir) || !DirectoryExists(webdir)) {
            log("Error: could not resolve symbols/web folder.");
            PromptOK("Could not resolve 'symbols/web' below the selected Input folder.\n"
                     "Please point Input to either the repo root or symbols/web directly.");
            return;
        }
    
        // Make sure we have JSON loaded (and the allJsonIconIds cache populated)
        if(IsEmpty(metaPath) || !FileExists(metaPath)) {
            log("Error: Input JSON is required for coverage check.");
            PromptOK("Input JSON (metadata or current_versions.json) is required for coverage check.");
            return;
        }
    
        if(metaPath != lastMetaPath || categories.IsEmpty() || allJsonIconIds.IsEmpty()) {
            log("Reloading Input JSON before coverage check ...");
            if(!LoadCategoriesFromJson(metaPath)) {
                log("Error: failed to load categories from Input JSON; aborting coverage check.");
                return;
            }
        }
    
        // --- 1) Collect icon IDs from symbols/web/<icon>/... ---
        Index<String> fsIcons;  // sanitized ids from folder names
    
        FindFile ff(AppendFileName(webdir, "*"));
        while(ff) {
            if(ff.IsDirectory()) {
                const String icon_dir = ff.GetName();
                String id = SanitizeId(icon_dir);
                if(!id.IsEmpty())
                    fsIcons.FindAdd(id);
            }
            ff.Next();
        }
    
        // --- 2) Compare sets: fsIcons vs allJsonIconIds ---
        Vector<String> fsOnly;
        Vector<String> jsonOnly;
    
        for(int i = 0; i < fsIcons.GetCount(); ++i) {
            const String& id = fsIcons[i];
            if(allJsonIconIds.Find(id) < 0)
                fsOnly.Add(id);
        }
    
        for(int i = 0; i < allJsonIconIds.GetCount(); ++i) {
            const String& id = allJsonIconIds[i];
            if(fsIcons.Find(id) < 0)
                jsonOnly.Add(id);
        }
    
        Sort(fsOnly);
        Sort(jsonOnly);
    
        log(Format("Coverage: symbols/web folders: %d, JSON icon IDs: %d",
                   fsIcons.GetCount(), allJsonIconIds.GetCount()));
        log(Format("  • In symbols/web but NOT in JSON: %d", fsOnly.GetCount()));
        log(Format("  • In JSON but NOT in symbols/web: %d", jsonOnly.GetCount()));
    
        if(!fsOnly.IsEmpty()) {
            log("  First FS-only icons (present on disk, missing in JSON):");
            for(int i = 0; i < fsOnly.GetCount() && i < 50; ++i)
                log("    " + fsOnly[i]);
            if(fsOnly.GetCount() > 50)
                log(Format("    ... (%d more)", fsOnly.GetCount() - 50));
        }
    
        if(!jsonOnly.IsEmpty()) {
            log("  First JSON-only icons (referenced in JSON, missing folders):");
            for(int i = 0; i < jsonOnly.GetCount() && i < 50; ++i)
                log("    " + jsonOnly[i]);
            if(jsonOnly.GetCount() > 50)
                log(Format("    ... (%d more)", jsonOnly.GetCount() - 50));
        }
    
        String summary = Format(
            "Coverage check complete.\n\n"
            "symbols/web folders: %d\n"
            "JSON icon IDs:      %d\n"
            "FS-only (on disk, not in JSON): %d\n"
            "JSON-only (in JSON, no folder): %d\n",
            fsIcons.GetCount(), allJsonIconIds.GetCount(),
            fsOnly.GetCount(), jsonOnly.GetCount()
        );
        PromptOK(DeQtf(summary));
    }

    void DoCheckMetaCoverage() {
        auto log = [&](const String& s) {
            AppendStatus(s);
            Ctrl::ProcessEvents();
        };

        log("");
        log(Format("=== Coverage check vs material_symbols_metadata.json (v%s) ===", kAppVersion));

        const String picked = ~editIn;

        if(IsEmpty(picked) || !DirectoryExists(picked)) {
            log("Error: input repo is not set or does not exist.");
            PromptOK("Please select a valid Input repo root (or symbols/web) first.");
            return;
        }

        String webdir = ResolveSymbolsWebDir(picked);
        if(IsNull(webdir) || !DirectoryExists(webdir)) {
            log("Error: could not resolve symbols/web folder.");
            PromptOK("Could not resolve 'symbols/web' below the selected Input folder.\n"
                     "Please point Input to either the repo root or symbols/web directly.");
            return;
        }
        if(!HasAtLeastOne24pxSvgSymbols(webdir)) {
            log("Error: no '*_24px.svg' files found under symbols/web.");
            PromptOK("No '*_24px.svg' files were found under:\n  " + webdir);
            return;
        }

        // Deduce repo_root from picked
        String base      = picked;
        String repo_root = base;
        String last      = GetFileName(base);
        String parent    = GetFileName(GetFileFolder(base));
        if(last == "web" && parent == "symbols")
            repo_root = GetFileFolder(GetFileFolder(base));
        else if(last == "symbols")
            repo_root = GetFileFolder(base);

        String meta_symbols = AppendFileName(repo_root, "material_symbols_metadata.json");
        if(!FileExists(meta_symbols)) {
            log("Error: material_symbols_metadata.json not found next to 'symbols' tree.");
            PromptOK("Could not find 'material_symbols_metadata.json' next to the 'symbols' tree.\n\n"
                     "Expected at:\n  " + meta_symbols);
            return;
        }

        // Collect icon IDs from symbols/web/<icon>/...
        Index<String> fsIcons;
        FindFile icon_ff(AppendFileName(webdir, "*"));
        while(icon_ff) {
            if(icon_ff.IsDirectory()) {
                const String icon_dir = icon_ff.GetName();
                String id = SanitizeId(icon_dir);
                if(!id.IsEmpty())
                    fsIcons.FindAdd(id);
            }
            icon_ff.Next();
        }

        log("Loading material_symbols_metadata.json for coverage ...");

        // Fill disk IDs for possible misc-category use
        ScanDiskIconIds(webdir);

        if(!LoadCategoriesFromJson(meta_symbols)) {
            log("Error: failed to load material_symbols_metadata.json; aborting coverage check.");
            return;
        }

        // Keep the UI's Input JSON field in sync with what we just used
        editMeta <<= meta_symbols;

        // Compare fsIcons vs allJsonIconIds (now from material_symbols_metadata.json)
        Vector<String> fsOnly;
        Vector<String> jsonOnly;

        for(int i = 0; i < fsIcons.GetCount(); ++i) {
            const String& id = fsIcons[i];
            if(allJsonIconIds.Find(id) < 0)
                fsOnly.Add(id);
        }

        for(int i = 0; i < allJsonIconIds.GetCount(); ++i) {
            const String& id = allJsonIconIds[i];
            if(fsIcons.Find(id) < 0)
                jsonOnly.Add(id);
        }

        Sort(fsOnly);
        Sort(jsonOnly);

        log(Format("Coverage vs material_symbols_metadata.json: symbols/web folders: %d, metadata icon IDs: %d",
                   fsIcons.GetCount(), allJsonIconIds.GetCount()));
        log(Format("  • In symbols/web but NOT in metadata: %d", fsOnly.GetCount()));
        log(Format("  • In metadata but NOT in symbols/web: %d", jsonOnly.GetCount()));

        if(!fsOnly.IsEmpty()) {
            log("  First FS-only icons (present on disk, missing in material_symbols_metadata.json):");
            for(int i = 0; i < fsOnly.GetCount() && i < 50; ++i)
                log("    " + fsOnly[i]);
            if(fsOnly.GetCount() > 50)
                log(Format("    ... (%d more)", fsOnly.GetCount() - 50));
        }

        if(!jsonOnly.IsEmpty()) {
            log("  First JSON-only icons (listed in material_symbols_metadata.json, missing folders):");
            for(int i = 0; i < jsonOnly.GetCount() && i < 50; ++i)
                log("    " + jsonOnly[i]);
            if(jsonOnly.GetCount() > 50)
                log(Format("    ... (%d more)", jsonOnly.GetCount() - 50));
        }

        String summary = Format(
            "Coverage vs material_symbols_metadata.json complete.\n\n"
            "symbols/web folders: %d\n"
            "metadata icon IDs:   %d\n"
            "FS-only (on disk, not in metadata): %d\n"
            "JSON-only (in metadata, no folder): %d\n",
            fsIcons.GetCount(), allJsonIconIds.GetCount(),
            fsOnly.GetCount(), jsonOnly.GetCount()
        );
        PromptOK(DeQtf(summary));
    }

    void BuildUI() {
        headerCard
            .SetTitle("Material Symbols — SVG Extractor")
            .SetSubTitleFont(StdFont().Height(DPI(10)))
            .SetSubTitle("Extract + compress 24px.svg from Material Symbols symbols/web (outlined / rounded / sharp)")
            .SetHeaderAlign(StageCard::LEFT)
            .EnableCardFill(false).EnableCardFrame(false)
            .EnableHeaderFill(false)
            .SetHeaderGap(DPI(8))
            .SetHeaderInset(0, DPI(12), 0, 0)
            .SetStackNone()
            .EnableContentScroll(false)
            .EnableContentFill(true).EnableContentFrame(true)
            .SetContentCornerRadius(DPI(8))
            .SetContentFrameThickness(1);
        Add(headerCard.HSizePos(DPI(8), DPI(8)).VSizePos(DPI(8), DPI(8)));

        btnProcess.SetLabel("Process")
                  .SetStroke(0)
                  .SetBaseColors(SColorHighlight(), SColorShadow(), White)
                  .SetBadgeText("");
        btnExit   .SetLabel("Exit")
                  .SetStroke(0)
                  .SetBaseColors(Color(235,66,33), SColorShadow(), White);
        btnHelp   .SetLabel("Help")
                  .SetStroke(0)
                  .SetBaseColors(GrayColor(100), SColorShadow(), White);

        btnCheckMetaCoverage.SetLabel("Check Meta")
                            .SetStroke(0)
                            .SetBaseColors(GrayColor(140), SColorShadow(), White)
                            .SetBadgeText("");
        btnCheckMetaCoverage.Tip("Compare symbols/web folders with material_symbols_metadata.json IDs.\n"
                                 "Shows icons present on disk but missing material_symbols_metadata file,\n"
                                 "and icons in JSON that have no symbols/web folder.");
        btnCheckCoverage.SetLabel("Check")
                        .SetStroke(0)
                        .SetBaseColors(GrayColor(140), SColorShadow(), White)
                        .SetBadgeText("");
        btnCheckCoverage.Tip("Compare symbols/web folders with JSON icon IDs.\n"
                             "Shows icons present on disk but missing in JSON,\n"
                             "and icons in JSON that have no symbols/web folder.");

        btnProcess.WhenAction       = THISBACK(DoProcess);
        btnExit.WhenAction          = THISBACK(Close);
        btnHelp.WhenAction          = THISBACK(ShowHelp);
        btnCheckCoverage.WhenAction = THISBACK(DoCheckCoverage);
        btnCheckMetaCoverage.WhenAction = THISBACK(DoCheckMetaCoverage);

        headerCard.AddHeader(btnExit   .RightPos( ColPos(90,true), DPI(90)).TopPos(DPI(10), DPI(28)));
        headerCard.AddHeader(btnProcess.RightPos( ColPos(90),      DPI(90)).TopPos(DPI(10), DPI(28)));
        headerCard.AddHeader(btnCheckCoverage.RightPos( ColPos(90),      DPI(90)).TopPos(DPI(10), DPI(28)));
        headerCard.AddHeader(btnCheckMetaCoverage.RightPos( ColPos(90),      DPI(90)).TopPos(DPI(10), DPI(28)));
        headerCard.AddHeader(btnHelp   .RightPos( ColPos(90),      DPI(90)).TopPos(DPI(10), DPI(28)));

        lblIn.SetText("Input repo root (or symbols/web):");
        lblOut.SetText("Output folder:");
        lblMeta.SetText("Input JSON (required):");

        browseIn .SetLabel("...")
                 .SetBaseColors(SColorHighlight(), SColorShadow(), White)
                 .SetBadgeText("");
        browseOut.SetLabel("...")
                 .SetBaseColors(SColorHighlight(), SColorShadow(), White)
                 .SetBadgeText("");
        browseMeta.SetLabel("...")
                  .SetBaseColors(SColorHighlight(), SColorShadow(), White)
                  .SetBadgeText("");

        btnLoadCats.SetLabel("Load cats")
                   .SetStroke(0)
                   .SetBaseColors(SColorHighlight(), SColorShadow(), White)
                   .SetBadgeText("");

        browseIn.Tip("Browse to Material Symbols repo root or its 'symbols/web' folder.\n"
                     "The tool will auto-guess an adjacent Input JSON (preferring material_symbols_metadata.json or update/current_versions.json).");
        browseOut.Tip("Browse to output folder for generated icons_<category>.h headers.");
        browseMeta.Tip("Browse to Input JSON (metadata or current_versions.json) file.");
        btnLoadCats.Tip("Load categories from the Input JSON.");

        headerCard.AddFixed(lblIn   .LeftPos( ColPos(220,true), DPI(200)).TopPos(DPI(15),  DPI(24)));
        headerCard.AddFixed(editIn  .LeftPos( ColPos(480),      DPI(480)).TopPos(DPI(15),  DPI(24)));
        headerCard.AddFixed(browseIn.LeftPos( ColPos(40),       DPI(40)) .TopPos(DPI(15),  DPI(24)));

        headerCard.AddFixed(lblOut   .LeftPos( ColPos(220,true), DPI(200)).TopPos(DPI(47), DPI(24)));
        headerCard.AddFixed(editOut  .LeftPos( ColPos(480),      DPI(480)).TopPos(DPI(47), DPI(24)));
        headerCard.AddFixed(browseOut.LeftPos( ColPos(40),       DPI(40)) .TopPos(DPI(47), DPI(24)));

        headerCard.AddFixed(lblMeta   .LeftPos( ColPos(220,true), DPI(200)).TopPos(DPI(79), DPI(24)));
        headerCard.AddFixed(editMeta  .LeftPos( ColPos(420),      DPI(420)).TopPos(DPI(79), DPI(24)));
        headerCard.AddFixed(browseMeta.LeftPos( ColPos(40),       DPI(40)) .TopPos(DPI(79), DPI(24)));
        headerCard.AddFixed(btnLoadCats.LeftPos( ColPos(90),      DPI(80)) .TopPos(DPI(79), DPI(24)));

        lblCatLimit.SetText("Limit Categories (0 = no limit):");
        spinCatLimit.Min(0).Max(999).SetData(0);
        spinCatLimit.Tip("Emit at most N non-empty categories (0 = no limit).");

        lblIconLimit.SetText("Limit Icons (0 = no limit):");
        spinIconLimit.Min(0).Max(1000000).SetData(0);
        spinIconLimit.Tip("Hard cap on how many icons (unique icon names) are emitted in total.\n"
                          "0 = no limit.");

        colX = colStart;
        headerCard.AddFixed(lblCatLimit.LeftPos( ColPos(220, true), DPI(200)).TopPos(DPI(115), DPI(24)));
        headerCard.AddFixed(spinCatLimit.LeftPos( ColPos(100),       DPI(60)) .TopPos(DPI(115), DPI(24)));
        headerCard.AddFixed(lblIconLimit.LeftPos( ColPos(130),      DPI(130)).TopPos(DPI(115), DPI(24)));
        headerCard.AddFixed(spinIconLimit.LeftPos( ColPos(80),      DPI(60)) .TopPos(DPI(115), DPI(24)));

        lblCatSummary.SetText("No Input JSON loaded.");
        lblCatSummary.SetInk(SColorDisabled());
        lblCatSummary.SetFont(StdFont().Height(DPI(10)));
        headerCard.AddFixed(lblCatSummary.LeftPos( ColPos(220), DPI(220)).TopPos(DPI(120), DPI(15)));

        optionsY = 150;

        all.SetLabel("All");
        all.Set(1);
        all.Tip("Tick/untick to enable or disable all categories in one go.");
        all.WhenAction = [=] {
            bool v = all.Get();
            for(Option& o : optionCat)
                o.Set(v);
        };

        colX = colStart;
        headerCard.AddFixed(all.LeftPos(ColPos(80,true), DPI(80)).TopPos(DPI(optionsY), DPI(22)));

        RebuildCategoryOptions();

        status.SetReadOnly();
        status.SetFrame(NullFrame());
        status.SetFont(Monospace(10));
        status.Transparent(false);
        status.Tip("Append-only log of extractor actions.");
        
        
      

		headerCard.AddFixed(  
		    status.HSizePos(DPI(10), DPI(10))
		          .VSizePos(DPI(280), DPI(10))   // top = statusTop, bottom = 10  
		);


        browseIn << [=]{
            FileSel fs;
            fs.ActiveDir(~editIn);
            if(fs.ExecuteSelectDir()) {
                editIn <<= ~fs;
                WarnIfStructureUnexpected(~editIn);

                String base = (String)~editIn;
                
                // Normalize to repo root as in constructor
                String repo_root = base;
                String last   = GetFileName(base);
                String parent = GetFileName(GetFileFolder(base));
                if(last == "web" && parent == "symbols")
                    repo_root = GetFileFolder(GetFileFolder(base));
                else if(last == "symbols")
                    repo_root = GetFileFolder(base);
                
                String guess_meta_symbols = AppendFileName(repo_root, "material_symbols_metadata.json");
                String guess_update       = AppendFileName(AppendFileName(repo_root, "update"), "current_versions.json");
                String guess_root         = AppendFileName(repo_root, "metadata.json");
                String guess_src          = AppendFileName(AppendFileName(repo_root, "src"), "metadata.json");
                
                String meta_guess;
                if(FileExists(guess_meta_symbols))
                    meta_guess = guess_meta_symbols;
                else if(FileExists(guess_update))
                    meta_guess = guess_update;
                else if(FileExists(guess_root))
                    meta_guess = guess_root;
                else if(FileExists(guess_src))
                    meta_guess = guess_src;
                else
                    meta_guess = guess_meta_symbols;
                
                editMeta <<= meta_guess;
                if(FileExists(meta_guess))
                    LoadCategoriesFromJson(meta_guess);
            }
        };
        browseOut << [=]{
            FileSel fs;
            fs.ActiveDir(~editOut);
            if(fs.ExecuteSelectDir())
                editOut <<= ~fs;
        };

		browseMeta << [=]{
		    FileSel fs;
		    fs.Type("JSON files", "*.json");
		
		    String base = ~editMeta;
		    if(IsEmpty(base))
		        base = ~editIn;
		
		    // If base is a file path, use its folder; otherwise use base as-is.
		    if(!IsEmpty(base) && !DirectoryExists(base))
		        base = GetFileFolder(base);
		
		    fs.ActiveDir(base);
		    if(fs.ExecuteOpen()) {
		        editMeta <<= ~fs;
		        LoadCategoriesFromJson(~editMeta);
		    }
		};

        btnLoadCats << [=]{
            String path = ~editMeta;
            if(IsEmpty(path)) {
                PromptOK("Please specify Input JSON first.");
                return;
            }
            LoadCategoriesFromJson(path);
        };
    }


    bool Key(dword key, int count) override {
        if(key == K_ENTER) {
            DoProcess();
            return true;
        }
        if(key == K_ESCAPE) {
            Close();
            return true;
        }
        return TopWindow::Key(key, count);
    }


    // ---- string sanitization for C++ identifiers and icon IDs ----
    static String SanitizeId(String s) {
        for(int i = 0; i < s.GetCount(); ++i) {
            char c = s[i];
            if(!(IsAlNum(c) || c == '_'))
                s.Set(i, '_');
        }
        // We no longer prefix '_' for leading digits – folder names in symbols/web
        // legitimately start with digits for icons like "360", "3d_rotation".
        return ToLower(s);
    }

    // ---- map web style folder name → short key ----
    static String MapWebStyleToKey(const String& style_dir_name) {
        const String n = ToLower(style_dir_name);
        if(n == "materialsymbolsoutlined") return "outlined";
        if(n == "materialsymbolsrounded")  return "rounded";
        if(n == "materialsymbolssharp")    return "sharp";
        return String();
    }

    // ---- reverse: short key → web dir name ----
    static String StyleKeyToWebDir(const String& key) {
        if(key == "outlined") return "materialsymbolsoutlined";
        if(key == "rounded")  return "materialsymbolsrounded";
        if(key == "sharp")    return "materialsymbolssharp";
        return String();
    }

    // ---- lightweight SVG minifier ----
    static String MinifySvg(const String& in) {
        String out;
        out.Reserve(in.GetCount());
        bool in_space = false;
        for(int i = 0; i < in.GetCount(); ++i) {
            byte c = in[i];
            if(c == '\r' || c == '\n' || c == '\t')
                c = ' ';
            if(c == ' ') {
                if(in_space)
                    continue;
                in_space = true;
            }
            else
                in_space = false;
            out.Cat(c);
        }
        String out2;
        out2.Reserve(out.GetCount());
        for(int i = 0; i < out.GetCount(); ++i) {
            byte c = out[i];
            if((c == ' ' || c == '\t') && i + 1 < out.GetCount() &&
               (out[i + 1] == '>' || out[i + 1] == '<'))
                continue;
            if((c == '<' || c == '>' ) && i + 1 < out.GetCount() &&
               (out[i + 1] == ' ' || out[i + 1] == '\t')) {
                out2.Cat(c);
                ++i;
                continue;
            }
            out2.Cat(c);
        }
        return TrimBoth(out2);
    }

    // ---- probe: do we find *any* <icon>_24px.svg in a supported web style? ----
    bool HasAtLeastOne24pxSvgSymbols(const String& webdir) {
        FindFile icon_ff(AppendFileName(webdir, "*"));
        while(icon_ff) {
            if(icon_ff.IsDirectory()) {
                const String icon_name = icon_ff.GetName();
                const String icon_path = AppendFileName(webdir, icon_name);

                FindFile style_ff(AppendFileName(icon_path, "*"));
                while(style_ff) {
                    if(style_ff.IsDirectory()) {
                        const String st_dir = style_ff.GetName();

                        // Only consider real Material Symbols style dirs
                        if(IsNull(MapWebStyleToKey(st_dir))) {
                            style_ff.Next();
                            continue;
                        }

                        const String style_path = AppendFileName(icon_path, st_dir);
                        const String svg_24    = AppendFileName(style_path,
                                                                icon_name + "_24px.svg");
                        if(FileExists(svg_24))
                            return true;
                    }
                    style_ff.Next();
                }
            }
            icon_ff.Next();
        }
        return false;
    }

    // ---- resolve user-picked dir to symbols/web ----
    String ResolveSymbolsWebDir(const String& picked) {
        const String base = picked;

        // Case 1: user already points to .../symbols/web
        if(DirectoryExists(base)) {
            const String last   = GetFileName(base);
            const String parent = GetFileName(GetFileFolder(base));
            if(last == "web" && parent == "symbols")
                return base;
        }

        // Case 2: user points to .../symbols
        {
            const String last = GetFileName(base);
            if(last == "symbols") {
                const String web = AppendFileName(base, "web");
                if(DirectoryExists(web))
                    return web;
            }
        }

        // Case 3: user points to repo root .../material-design-icons
        {
            const String symbols = AppendFileName(base, "symbols");
            const String web     = AppendFileName(symbols, "web");
            if(DirectoryExists(web))
                return web;
        }

        return Null; // not recognized
    }

    // ---- write header for a category bucket ----
    static bool WriteCategoryHeader(const String& outdir,
                                    const String& cat_san,
                                    const String& cat_orig,
                                    const VectorMap<String, VectorMap<String, String>>& bucket,
                                    const VectorMap<String, String>& id2orig)
    {
        int icon_total = 0;
        for(int si = 0; si < bucket.GetCount(); ++si)
            icon_total += bucket[si].GetCount();
        if(icon_total == 0)
            return true;

        const String fn     = AppendFileName(outdir, "icons_" + cat_san + ".h");
        const String tmp_fn = fn + ".tmp";
        const String GUARD  = "UPP_MATERIAL_SYMBOLS_ICONS_" + ToUpper(cat_san) + "_H";

        String h;
        h << "// Auto-generated by Material Symbols SVG Extractor (v" << kAppVersion << ")\n"
          << "// " << kFormatSignature << "\n"
          << "// Category (logical): " << cat_orig << "\n"
          << "// Source layout (symbols/web): symbols/web/<icon>/<style>/<icon>_24px.svg\n"
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

        Vector<String> styles;
        for(int si = 0; si < bucket.GetCount(); ++si)
            styles.Add(bucket.GetKey(si));
        Sort(styles);

        for(const String& st_key : styles) {
            int si = bucket.Find(st_key);
            Vector<String> icon_ids;
            for(int ii = 0; ii < bucket[si].GetCount(); ++ii)
                icon_ids.Add(bucket[si].GetKey(ii));
            Sort(icon_ids);

            for(const String& id : icon_ids) {
                const String& svg       = bucket[si].Get(id);
                const String  z         = ZCompress(MinifySvg(svg));
                const String  b64       = Base64Encode(z);
                const String  icon_orig = id2orig.Get(id, id);
                const String  src_style = StyleKeyToWebDir(st_key);
                const String  blob_sym  = cat_san + "_" + st_key + "_" + id + "_b64z";

                h << "// " << id << " [" << st_key << "]  source: "
                  << "symbols/web/" << icon_orig << "/" << src_style << "/" << icon_orig << "_24px.svg\n";
                h << "static const char " << blob_sym << "[] =\n    \"";
                for(int i = 0; i < b64.GetCount(); ++i) {
                    h.Cat(b64[i]);
                    if((i + 1) % 80 == 0 && i + 1 < b64.GetCount())
                        h << "\"\n    \"";
                }
                h << "\";\n\n";
            }
        }

        h << "static const BaseSVGIcon kIcons[] = {\n";
        for(const String& st_key : styles) {
            int si = bucket.Find(st_key);
            Vector<String> icon_ids;
            for(int ii = 0; ii < bucket[si].GetCount(); ++ii)
                icon_ids.Add(bucket[si].GetKey(ii));
            Sort(icon_ids);

            for(const String& id : icon_ids) {
                const String icon_orig  = id2orig.Get(id, id);
                const String src_style  = StyleKeyToWebDir(st_key);
                const String blob_sym   = cat_san + "_" + st_key + "_" + id + "_b64z";
                const String source_rel = "symbols/web/" + icon_orig + "/" + src_style + "/" + icon_orig + "_24px.svg";

                h << "    { \"" << cat_san << "\", " << styleEnum(st_key) << ", \"" << id << "\", "
                  << "\"" << source_rel << "\", " << blob_sym << " },\n";
            }
        }
        h << "};\n\n"
          << "static const int kIconCount = (int)(sizeof(kIcons) / sizeof(kIcons[0]));\n\n"
          << "} // namespace " << cat_san << "\n\n"
          << "#endif // " << GUARD << "\n";

        if(!SaveFile(tmp_fn, h))
            return false;

        {
            String chk = LoadFile(tmp_fn);
            if(chk.Find(kFormatSignature) < 0)
                return false;
        }

        if(FileExists(fn))
            FileDelete(fn);
        if(!FileMove(tmp_fn, fn))
            return false;

        return true;
    }

    // ---- status log helpers ----
    void ClearStatus() { status.Set(WString()); }

    void AppendStatus(const String& t) {
        String cur = status.Get();
        if(!cur.IsEmpty())
            cur.Cat('\n');
        cur.Cat(String(t));
        status.Set(cur);
        status.SetCursor(status.GetLength());
    }

    void ShowHelp() {
        String t;
        t << Format("Material Symbols SVG Extractor — v%s (web mode)\n\n", kAppVersion)
          << "Purpose:\n"
          << "  • Scan 'symbols/web' for '*_24px.svg' in styles: outlined / rounded / sharp.\n"
          << "  • Drive per-category icon headers from an external JSON metadata file.\n\n"
          << "Output:\n"
          << "  • Emits one header per logical category: icons_<category>.h\n"
          << "  • Each header defines:\n"
          << "        enum Style : uint8 { OUTLINED, ROUNDED, SHARP };\n"
          << "        struct BaseSVGIcon {\n"
          << "            const char* category;\n"
          << "            Style       style;\n"
          << "            const char* name;\n"
          << "            const char* source;\n"
          << "            const char* b64zIcon;\n"
          << "        };\n"
          << "        static const BaseSVGIcon kIcons[];\n"
          << "        static const int         kIconCount;\n\n"
          << "Decode on demand in your app:\n"
          << "  String svg = ZDecompress(Base64Decode(kIcons[i].b64zIcon));\n\n"
          << "Input JSON (required):\n"
          << "  • Supplies categories + per-category icon IDs.\n"
          << "  • Supported formats include:\n"
          << "      (A) material_symbols_metadata.json:\n"
          << "            { \"icons\": [ { \"name\": \"...\", \"categories\": [\"Audio & Video\", ...] }, ... ] }\n"
          << "      (B) big maps:\n"
          << "            { \"icons\": { \"search\": { \"categories\": [...] }, ... } }\n"
          << "      (C) compact current_versions.json style:\n"
          << "            { \"symbols::zoom_out_map\": 329, ... }  // prefix before \"::\" is the category.\n\n"
          << "Coverage helpers:\n"
          << "  • 'Check' uses the currently loaded Input JSON (edit box) and compares its IDs\n"
          << "    to the symbols/web folders.\n"
          << "  • 'Check Meta' locates material_symbols_metadata.json next to the 'symbols' tree\n"
          << "    and compares its IDs to the symbols/web folders (regardless of the Input JSON\n"
          << "    currently selected).\n\n"
          << "Expected layout (web mode):\n"
          << "  symbols/web/<icon>/<style>/<icon>_24px.svg\n"
          << "    style names: materialsymbolsoutlined, materialsymbolsrounded, materialsymbolssharp.\n";
        PromptOK(DeQtf(t));
    }

    // ---- guard: ensure the selected folder resolves to symbols/web ----
    bool WarnIfStructureUnexpected(const String& picked) {
        String webdir = ResolveSymbolsWebDir(picked);
      
        String msg = Format("picked %s", picked);
        AppendStatus(msg);
        msg = Format("resolved symbols/web dir: %s", webdir);
        AppendStatus(msg);

        if(IsNull(webdir) || !DirectoryExists(webdir)) {
            String m;
            m << "Please select the repository root or the 'symbols/web' folder.\n\n"
              << "Expected layout:\n"
              << "  <repo_root>/symbols/web/<icon>/<style>/<icon>_24px.svg\n\n"
              << "Selected path:\n  " << picked;
            PromptOK(DeQtf(m));
            allDiskIconIds.Clear();
            return true;
        }
        if(!HasAtLeastOne24pxSvgSymbols(webdir)) {
            String m;
            m << "No '*_24px.svg' files were found under:\n  " << webdir << "\n\n"
              << "Expected layout:\n"
              << "  symbols/web/<icon>/<style>/<icon>_24px.svg\n"
              << "Styles: materialsymbolsoutlined, materialsymbolsrounded, materialsymbolssharp.";
            PromptOK(DeQtf(m));
            allDiskIconIds.Clear();
            return true;
        }

        // At this point the structure looks correct; build disk icon ID set.
        ScanDiskIconIds(webdir);

        return false;
    }
    
    // After both JSON and disk scans are done, call this to:
    //  - create / extend a "misc" category with all disk icons that are NOT in allJsonIconIds
    //  - optionally report JSON-only icons (present in JSON but not on disk)
    void AddMiscCategoryFromDisk() {
        if(allDiskIconIds.IsEmpty() || allJsonIconIds.IsEmpty())
            return;

        // Build lookup for JSON IDs
        Index<String> jsonSet;
        jsonSet.Reserve(allJsonIconIds.GetCount());
        for(int i = 0; i < allJsonIconIds.GetCount(); ++i)
            jsonSet.FindAdd(allJsonIconIds[i]);

        // 1) Disk but not JSON -> misc
        Vector<String> diskNotJson;
        for(int i = 0; i < allDiskIconIds.GetCount(); ++i) {
            const String& id = allDiskIconIds[i];
            if(jsonSet.Find(id) < 0)
                diskNotJson.Add(id);
        }

        // Optional: JSON but not disk (only for info / debugging)
        Index<String> diskSet;
        diskSet.Reserve(allDiskIconIds.GetCount());
        for(int i = 0; i < allDiskIconIds.GetCount(); ++i)
            diskSet.FindAdd(allDiskIconIds[i]);

        int jsonOnlyCount = 0;
        for(int i = 0; i < allJsonIconIds.GetCount(); ++i) {
            const String& id = allJsonIconIds[i];
            if(diskSet.Find(id) < 0)
                ++jsonOnlyCount;
        }

        if(jsonOnlyCount > 0)
            AppendStatus(Format("JSON icons not found on disk: %d", jsonOnlyCount));

        if(diskNotJson.IsEmpty())
            return;

        // 2) Attach / create "misc" category
        String miscName = "misc";

        int miscIndex = -1;
        for(int i = 0; i < categories.GetCount(); ++i) {
            if(categories[i] == miscName) {
                miscIndex = i;
                break;
            }
        }

        if(miscIndex < 0) {
            miscIndex = categories.GetCount();
            categories.Add(miscName);
            catItemCounts.Add(0);
            catIconIds.Add();  // empty Index<String>
        }

        Index<String>& miscIcons = catIconIds[miscIndex];

        int newly_added = 0;
        for(const String& id : diskNotJson) {
            if(miscIcons.Find(id) < 0) {
                miscIcons.Add(id);
                ++catItemCounts[miscIndex];
                ++newly_added;
            }
        }

        if(newly_added > 0)
            AppendStatus(Format("Added %d disk-only icons to 'misc' category.", newly_added));
    }

    // Scan symbols/web/<icon>/... and build the set of sanitized icon IDs present on disk.
    void ScanDiskIconIds(const String& webdir) {
        allDiskIconIds.Clear();

        if(IsNull(webdir) || !DirectoryExists(webdir))
            return;

        FindFile ff(AppendFileName(webdir, "*"));
        while(ff) {
            if(ff.IsDirectory()) {
                const String icon_dir = ff.GetName();
                String id = SanitizeId(icon_dir);
                if(!id.IsEmpty())
                    allDiskIconIds.FindAdd(id);
            }
            ff.Next();
        }

        AppendStatus(Format("Scanned symbols/web: %d icon folders on disk.", allDiskIconIds.GetCount()));
    }
    
    // Normalize a JSON category label into an internal / directory-friendly name.
    // Examples:
    //   "Audio\u0026Video" -> "audio_video"
    //   "Audio&Video"      -> "audio_video"
    //   "Audio & Video"    -> "audio_video"
    //   "AV" / "av"        -> "audio_video"  (optional aliasing)
    //   "Ui Action"        -> "ui_action"
    //   "Maps"             -> "maps"
    //   "Images"/"image"   -> "image"
   // Normalize a JSON category label into an internal / directory-friendly name.
//
// Rules/examples you requested:
//   "Audio\u0026Video" / "Audio&Video" / "Audio & Video" / "av" -> "audio_video"
//   "Images" / "image"                                         -> "image"
//   "UI actions" / "UI Actions" / "UI_action" / "UI_actions"   -> "ui_action"
//   "Maps" / "maps"                                            -> "map"
//   "Actions" / "actions"                                      -> "action"
//   "Communicate"                                              -> "communicate"
//
// Generic rules:
//   - Trim spaces.
//   - Lowercase everything.
//   - Non [a-z0-9] → '_' (collapsed).
//   - Strip leading/trailing '_'.
//   - If result ends with 's' and is reasonably long, drop the final 's'.
String NormalizeCategoryForDir(const String& raw) {
    // JSON "Audio\u0026Video" comes into U++ as "Audio&Video".
    String s = ToLower(TrimBoth(raw));
    if(s.IsEmpty())
        return String();

    // --- Specific aliases first (override generic rules) ---

    // Audio / Video
    if(s == "audio&video" || s == "audio & video" || s == "audio / video" || s == "av" ||  s == "audiovideo")
        return "audio_video";

    // UI actions
    if(s == "ui action" || s == "ui actions" || s == "ui_action" || s == "ui_actions" || s == "uiaction" )
        return "ui_action";

    // Image / Images
    if(s == "image" || s == "images")
        return "image";

    // You can add more explicit aliases here if needed later, but the
    // generic rule below already covers "maps"->"map", "actions"->"action",
    // "communicate"->"communicate", etc.

    // --- Generic normalization: [a-z0-9] kept, others → "_" (collapsed) ---

    String out;
    bool last_underscore = false;

    for(int i = 0; i < s.GetCount(); ++i) {
        int c = s[i];
        if(IsAlNum(c)) {
            out.Cat((char)ToLower(c));
            last_underscore = false;
        }
        else {
            if(!last_underscore && !out.IsEmpty()) {
                out.Cat('_');
                last_underscore = true;
            }
        }
    }

    // Strip trailing underscores
    while(!out.IsEmpty() && out[out.GetCount() - 1] == '_')
        out.Trim(out.GetCount() - 1);

    // Simple plural → singular: drop a final 's' for longer tokens.
    // This turns:
    //   "maps"     -> "map"
    //   "actions"  -> "action"
    //   "places"   -> "place"
    if(out.GetCount() > 3 && out[out.GetCount() - 1] == 's')
        out.Trim(out.GetCount() - 1);

    if(out.IsEmpty())
        out = "misc";

    return out;
}

// ---- category JSON loader (handles Google Fonts metadata + compact variants) ----  
// Assumes these members exist on IconExtractor:
//
// Vector<String>       categories;
// Vector< Index<String> > catIconIds;
// Vector<int>          catItemCounts;
// Index<String>        allJsonIconIds;
//
// and that you have #include <Core/JSON.h> somewhere in the .cpp.
bool LoadCategoriesFromJson(const String& path)
{
    auto log = [&](const String& s) {
        AppendStatus(s);
        Ctrl::ProcessEvents();
    };

    log("");
    log(Format("=== Loading Input JSON: %s ===", path));

    categories.Clear();
    catIconIds.Clear();
    catItemCounts.Clear();
    allJsonIconIds.Clear();
    lastJsonItemCount = 0;

    if(IsEmpty(path) || !FileExists(path)) {
        log("ERROR: Input JSON file does not exist: " + path);
        lblCatSummary.SetText("No Input JSON loaded.");
        lblCatSummary.SetInk(SColorDisabled());
        return false;
    }

    // --- Read file ---------------------------------------------------------
    String json = LoadFile(path);
    if(IsNull(json)) {
        log("ERROR: Failed to read Input JSON file: " + path);
        lblCatSummary.SetText("Failed to read Input JSON.");
        lblCatSummary.SetInk(SRed());
        return false;
    }

    // --- Parse JSON --------------------------------------------------------
    Value root = ParseJSON(json);
    if(IsError(root)) {
        String err = GetErrorText(root);
        log("ERROR: JSON parse failed for: " + path);
        log("       " + err);
        lblCatSummary.SetText("Input JSON parse error.");
        lblCatSummary.SetInk(SRed());
        PromptOK(DeQtf("JSON parse failed for:\n  " + path + "\n\nError:\n" + err));
        return false;
    }

    // Expect material_symbols_metadata*.json-style:
    // { "host": ..., "asset_url_pattern": ..., "families": [...],
    //   "icons": [ { "name": "...", "categories": [ ... ], ... }, ... ] }
    ValueMap top = root;
    Value    iconsVal = top["icons"];
    ValueArray icons = iconsVal; // for this file, "icons" is an array

    if(icons.GetCount() == 0) {
        log("ERROR: Input JSON has no 'icons' array or it is empty.");
        log("ERROR: No usable categories / icon names were found in the Input JSON.");
        lblCatSummary.SetText("Input JSON has no 'icons' array.");
        lblCatSummary.SetInk(SRed());
        return false;
    }

    lastJsonItemCount = icons.GetCount();

    // --- Build category → icon-name index ----------------------------------
    for(int i = 0; i < icons.GetCount(); ++i) {
        ValueMap icon = icons[i];

        String name = icon["name"];
        if(IsNull(name) || name.IsEmpty())
            continue;

        // Deduplicate all icon IDs across the JSON
        allJsonIconIds.FindAdd(name);

        ValueArray cats = icon["categories"];
        if(cats.GetCount() == 0)
            continue;

      //  for(int ci = 0; ci < cats.GetCount(); ++ci) {
      //      String cat = cats[ci];
      //      if(IsNull(cat) || cat.IsEmpty())
      //          continue;

        for(int ci = 0; ci < cats.GetCount(); ++ci) {
            String raw_cat = cats[ci];
            if(IsNull(raw_cat) || raw_cat.IsEmpty())
                continue;

           // *** key point: normalize JSON category → internal name
            String cat = NormalizeCategoryForDir(raw_cat);
            if(cat.IsEmpty())
                continue;


            // Find or create category slot
            int catIndex = -1;
            for(int k = 0; k < categories.GetCount(); ++k) {
                if(categories[k] == cat) {
                    catIndex = k;
                    break;
                }
            }
            if(catIndex < 0) {
                catIndex = categories.GetCount();
                categories.Add(cat);
                catIconIds.Add();   // new empty Index<String> for this category
            }

            // Add icon name to this category (Index<> deduplicates)
            catIconIds[catIndex].FindAdd(name);
        }
    }

    // --- Finalize counts / sanity check ------------------------------------
    for(int i = 0; i < catIconIds.GetCount(); ++i)
        catItemCounts.Add(catIconIds[i].GetCount());

    if(categories.IsEmpty() || allJsonIconIds.IsEmpty()) {
        log("ERROR: No usable categories / icon names were found in the Input JSON.");
        lblCatSummary.SetText("No categories / icons found in Input JSON.");
        lblCatSummary.SetInk(SRed());
        return false;
    }

    int totalPerCat = 0;
    for(int i = 0; i < catItemCounts.GetCount(); ++i)
        totalPerCat += catItemCounts[i];

    log(Format("Loaded %d categories from metadata JSON, %d unique icon names, %d category-memberships.",
               categories.GetCount(), allJsonIconIds.GetCount(), totalPerCat));

    // --- Update UI summary + state -----------------------------------------
    lastMetaPath = path;

    String summary = Format("%s — %d categories, %d JSON rows, %d unique icon IDs",
                            GetFileName(path),
                            categories.GetCount(),
                            lastJsonItemCount,
                            allJsonIconIds.GetCount());
    lblCatSummary.SetText(summary);
    lblCatSummary.SetInk(SColorText());

    // Rebuild the category checkboxes in the UI
    RebuildCategoryOptions();

    return true;
}

    void RebuildCategoryOptions() {
        for(int i = 0; i < optionCat.GetCount(); ++i)
            optionCat[i].Remove();
        optionCat.Clear();

        colX = colStart + 10;
        all.LeftPos(ColPos(80, true), DPI(80)).TopPos(DPI(optionsY), DPI(22));
        all.Set(1);

        int y = optionsY;

        for(int i = 0; i < categories.GetCount(); ++i) {
            const String& name  = categories[i];
            int count           = (i < catItemCounts.GetCount() ? catItemCounts[i] : 0);
            String label        = count > 0 ? Format("%s (%d)", name, count) : name;

            Option& o = optionCat.Add();
            o.Set(1);
            o.SetLabel(label);
            o.Tip(Format("Category '%s' (%d item%s) in JSON.",
                         name, count, count == 1 ? "" : "s"));

            headerCard.AddFixed(o.LeftPos( ColPos(122), DPI(122)).TopPos(DPI(y), DPI(22)) );
            if(colX > 800) {
                colX = colStart + 10;
                ColPos(80, true);
                y += 24;
            }
        }
        headerCard.Refresh();
    }

    Index<String> BuildEnabledSetLower() const {
        Index<String> out;
        int checked = 0;
        for(int i = 0; i < optionCat.GetCount() && i < categories.GetCount(); ++i) {
            const bool on = optionCat[i].Get();
            if(on) {
                String name = ToLower(categories[i]);
                out.Add(name);
                ++checked;
            }
        }
        if(checked == optionCat.GetCount()) {
            out.Clear();
        }
        return out;
    }

    static bool IsAllowedCategory(const String& cat_orig,
                                  const String& cat_san,
                                  const Index<String>& enabled) {
        if(enabled.IsEmpty())
            return true;
        String lo = ToLower(cat_orig);
        String ls = ToLower(cat_san);
        return enabled.Find(lo) >= 0 || enabled.Find(ls) >= 0;
    }

    // Apply global icon limit to a single category bucket.
    static void ApplyIconLimitToCategory(Cat& c, int& icon_limit_remaining) {
        if(icon_limit_remaining <= 0)
            return;

        Index<String> all_ids;
        for(int si = 0; si < c.bucket.GetCount(); ++si) {
            const VectorMap<String, String>& m = c.bucket[si];
            for(int ii = 0; ii < m.GetCount(); ++ii)
                all_ids.FindAdd(m.GetKey(ii));
        }

        const int total_ids = all_ids.GetCount();
        if(total_ids <= 0)
            return;

        const int allowed = min(icon_limit_remaining, total_ids);
        if(allowed <= 0)
            return;

        if(allowed < total_ids) {
            Vector<String> ordered;
            ordered.SetCount(total_ids);
            for(int i = 0; i < total_ids; ++i)
                ordered[i] = all_ids[i];
            Sort(ordered);

            Index<String> keep;
            for(int i = 0; i < allowed; ++i)
                keep.Add(ordered[i]);

            for(int si = 0; si < c.bucket.GetCount(); ++si) {
                const VectorMap<String, String>& old_m = c.bucket[si];
                VectorMap<String, String>        new_m;
                new_m.Reserve(old_m.GetCount());

                for(int ii = 0; ii < old_m.GetCount(); ++ii) {
                    const String& id = old_m.GetKey(ii);
                    if(keep.Find(id) >= 0)
                        new_m.Add(id, old_m[ii]);
                }

                c.bucket[si] = pick(new_m);
            }

            {
                const VectorMap<String, String>& old_id2orig = c.id2orig;
                VectorMap<String, String>        new_id2orig;
                new_id2orig.Reserve(old_id2orig.GetCount());

                for(int i = 0; i < old_id2orig.GetCount(); ++i) {
                    const String& id = old_id2orig.GetKey(i);
                    if(keep.Find(id) >= 0)
                        new_id2orig.Add(id, old_id2orig[i]);
                }

                c.id2orig = pick(new_id2orig);
            }
        }

        icon_limit_remaining -= allowed;
        if(icon_limit_remaining < 0)
            icon_limit_remaining = 0;
    }

    void DoProcess() {
        auto log = [&](const String& s) {
            AppendStatus(s);
            Ctrl::ProcessEvents();
        };
    
        log("");
        log(Format("=== Run (v%s, web mode) ===", kAppVersion));
    
        String picked   = ~editIn;
        String outdir   = ~editOut;
        String metaPath = ~editMeta;
    
        if(IsEmpty(picked) || IsEmpty(outdir)) {
            log("Error: select input repo and output folder.");
            PromptOK("Please select both:\n\n  • Input repo root (or symbols/web)\n  • Output folder");
            return;
        }
        if(!DirectoryExists(picked)) {
            log("Error: input directory does not exist.");
            PromptOK("Input directory does not exist:\n  " + picked);
            return;
        }
        if(!DirectoryExists(outdir) && !RealizeDirectory(outdir)) {
            log("Error: output directory cannot be created.");
            PromptOK("Output directory cannot be created:\n  " + outdir);
            return;
        }
    
        if(IsEmpty(metaPath)) {
            log("Error: Input JSON is required but not set.");
            PromptOK("Input JSON (metadata or current_versions.json) is required.\n\n"
                     "Set the JSON file that describes icons and categories.");
            return;
        }
    
        String webdir = ResolveSymbolsWebDir(picked);
        if(IsNull(webdir)) {
            log("Locating directories ... FAILED");
            PromptOK("Extractor targets 'symbols/web' layout:\n\n"
                     "  symbols/web/<icon>/<style>/<icon>_24px.svg");
            return;
        }
        if(!HasAtLeastOne24pxSvgSymbols(webdir)) {
            log("Locating directories ... FAILED (no '*_24px.svg' files found).");
            PromptOK("Did not find any '*_24px.svg' files in:\n  " + webdir);
            return;
        }
    
        log(Format("Locating directories ... FOUND [%s]", webdir));
    
        // Ensure categories + icon sets are loaded from JSON
        if(metaPath != lastMetaPath || categories.IsEmpty() || catIconIds.IsEmpty()) {
            log("Loading Input JSON for categories ...");
            if(!LoadCategoriesFromJson(metaPath)) {
                log("Error: failed to load categories from Input JSON; aborting.");
                return;
            }
        }
        if(categories.IsEmpty() || catIconIds.GetCount() != categories.GetCount()) {
            log("Error: category metadata is incomplete; aborting.");
            PromptOK("Category metadata is incomplete. Try reloading the Input JSON.");
            return;
        }
    
        // ---- Build enabled category set from checkboxes ----
        Index<String> enabledCats = BuildEnabledSetLower();
    
        if(!enabledCats.GetCount()) {
            log("Filter: all categories enabled (no filtering).");
        } else {
            String list;
            for(int i = 0; i < enabledCats.GetCount(); ++i) {
                if(i)
                    list << ", ";
                list << enabledCats[i];
            }
            log("Filter: enabled categories = [" + list + "]");
        }
    
        // Map filter → concrete category indices
        Vector<int> selectedCats;
        selectedCats.Reserve(categories.GetCount());
        for(int i = 0; i < categories.GetCount(); ++i) {
            const String lo = ToLower(categories[i]);
            if(enabledCats.IsEmpty() || enabledCats.Find(lo) >= 0)
                selectedCats.Add(i);
        }
        if(selectedCats.IsEmpty()) {
            log("Error: no categories are enabled after filtering; aborting.");
            PromptOK("No categories are enabled.\nTick at least one category or 'All'.");
            return;
        }
    
        const int cat_limit = (int)~spinCatLimit;
        if(cat_limit > 0)
            log(Format("Category limit: %d (0 = no limit).", cat_limit));
        else
            log("Category limit: none (0 = no limit).");
    
        const int  icon_limit        = (int)~spinIconLimit;
        const bool icon_limited      = icon_limit > 0;
        int        icon_limit_remain = icon_limit;
    
        if(icon_limited)
            log(Format("Icon limit: %d (hard cap on unique icon names).", icon_limit));
        else
            log("Icon limit: none (0 = no limit).");
    
        // ---- Union of all icon IDs referenced by the selected categories ----
        Index<String>  neededSet;
        Vector<String> neededIcons;
        neededIcons.Reserve(1024);
        
        for(int idx : selectedCats) {
            const Index<String>& ids = catIconIds[idx];
            for(int j = 0; j < ids.GetCount(); ++j) {
                const String& id = ids[j];
        
                // Only add the icon once across all selected categories
                if(neededSet.Find(id) >= 0)
                    continue;           // already queued
        
                neededSet.Add(id);      // first time we see this id
                neededIcons.Add(id);
            }
        }
        Sort(neededIcons);
        
        if(neededIcons.IsEmpty()) {
            log("Selected categories contain no icons in Input JSON; nothing to do.");
            PromptOK("Selected categories contain no icons in Input JSON.");
            return;
        }
    
        // ---- Category buckets (one Cat per JSON category) ----
        Vector<Cat> cats;
        cats.SetCount(categories.GetCount());
        for(int idx : selectedCats) {
            cats[idx].original  = categories[idx];
            cats[idx].sanitized = SanitizeId(categories[idx]);
        }
    
        int cats_seen              = selectedCats.GetCount();
        int cats_emitted           = 0;
        int cats_skipped_filtered  = categories.GetCount() - selectedCats.GetCount();
        int empty_selected_skipped = 0;
        int icons_seen             = 0;
        int files_found            = 0;
    
        // ---- Targeted scan: for each needed icon, probe symbols/web/<icon>/... only ----
        for(const String& icon_id : neededIcons) {
            const String icon_orig = icon_id; // directories use snake_case ids
            const String icon_path = AppendFileName(webdir, icon_orig);
    
            if(!DirectoryExists(icon_path)) {
                // Some icons in JSON might not exist under symbols/web.
                log(Format("   WARN: icon folder not found for '%s' (expected %s)",
                           icon_id, icon_path));
                continue;
            }
    
            ++icons_seen;
    
            // Load available styles once per icon
            VectorMap<String, String> styles_svgs;
    
            const char* styleKeys[] = { "outlined", "rounded", "sharp" };
            for(const char* sk : styleKeys) {
                String st_key  = sk;
                String st_dir  = StyleKeyToWebDir(st_key);
                String style_p = AppendFileName(icon_path, st_dir);
                String svg_24  = AppendFileName(style_p, icon_orig + "_24px.svg");
    
                if(FileExists(svg_24)) {
                    String svg = LoadFile(svg_24);
                    if(!svg.IsEmpty()) {
                        styles_svgs.GetAdd(st_key) = svg;
                        ++files_found;
                    } else {
                        log(Format("   WARN: failed to read %s", svg_24));
                    }
                }
            }
    
            if(styles_svgs.IsEmpty())
                continue;
    
            // Which selected categories want this icon?
            Vector<int> destCats;
            for(int ci : selectedCats) {
                if(catIconIds[ci].Find(icon_id) >= 0)
                    destCats.Add(ci);
            }
            if(destCats.IsEmpty())
                continue; // should not happen, but be safe
    
            for(int ci : destCats) {
                Cat& c = cats[ci];
    
                c.id2orig.GetAdd(icon_id) = icon_orig;
    
                for(int si = 0; si < styles_svgs.GetCount(); ++si) {
                    const String& st_key = styles_svgs.GetKey(si);
                    const String& svg    = styles_svgs[si];
    
                    int bsi = c.bucket.FindAdd(st_key);
                    c.bucket[bsi].GetAdd(icon_id) = svg;
                }
            }
        }
    
        // ---- Emit headers per category, respecting icon + category limits ----
        auto TotalIconsInCat = [&](const Cat& c)->int {
            int cnt = 0;
            for(int s = 0; s < c.bucket.GetCount(); ++s)
                cnt += c.bucket[s].GetCount();
            return cnt;
        };
    
        Sort(selectedCats, [&](int a, int b) {
            return ToLower(categories[a]) < ToLower(categories[b]);
        });
        
        for(int idx : selectedCats) {
            Cat& c = cats[idx];
            int cnt_before = TotalIconsInCat(c);
            if(cnt_before == 0) {
                ++empty_selected_skipped;
                continue;
            }
        
            if(icon_limited && icon_limit_remain > 0)
                ApplyIconLimitToCategory(c, icon_limit_remain);
        
            int cnt_after = TotalIconsInCat(c);
            if(cnt_after == 0) {
                ++empty_selected_skipped;
                continue;
            }
        
            if(cat_limit > 0 && cats_emitted >= cat_limit) {
                log(Format("Reached category limit (%d). '%s' will not be emitted.",
                           cat_limit, c.original));
                continue;
            }
        
            log(Format(" category: %s (%s)  → processing ...",
                       c.original, webdir));
        
            const bool   ok       = WriteCategoryHeader(outdir, c.sanitized, c.original,
                                                        c.bucket, c.id2orig);
            const String out_path = AppendFileName(outdir, "icons_" + c.sanitized + ".h");
            if(ok) {
                ++cats_emitted;
                log(Format("   DONE  [to file %s]", out_path));
            } else {
                log(Format("   ERROR [failed to write %s]", out_path));
            }
        }
    
        log("");
        log(Format("Summary: scanned %d categories, emitted %d, skipped filtered %d, skipped empty (selected) %d",
                   cats_seen, cats_emitted, cats_skipped_filtered, empty_selected_skipped));
        log(Format("         icons seen (folders): %d, 24px files loaded: %d",
                   icons_seen, files_found));
        log(Format("Output:  %s", outdir));
        if(icon_limited && icon_limit_remain == 0)
            log("Note: run stopped logically at icon limit.");
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