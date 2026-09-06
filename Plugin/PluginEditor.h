#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include "AmbientLookAndFeel.h"
#include "ambient/Modulation.h"
#include "ambient/ZPlane.h"
#include "ambient/Sources.h"
#include <vector>
#include <memory>
#include <map>
#include <set>

namespace edt { class RouteTable; }   // EditorCommon.h / EditorMatrix.cpp

// The editor groups the sections the way the signal flows:
//   VOICE (Oscillator, Air, Envelope, Filter, Space) -> FOREGROUND (Ensemble, Delay, Delay 2,
//   Near Reverb) -> out; FOREGROUND -> COSMOS (parallel, returns) ; FOREGROUND -> BACKGROUND
//   (Cloud, Far Reverb) -> out ; CONDUCTOR (Cluster Brain, Tuning) drives the voices;
//   MORPH blends two full presets. A routing map in the header draws exactly this.
class AmbientSynthEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit AmbientSynthEditor(AmbientSynthProcessor&);
    ~AmbientSynthEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    void chooseScalaFile();
    void chooseSourceFile(bool wavetable, int slot = -1);   // slot < 0: the clip goes into every slot
    void chooseImpulseFile(bool second = false);
    void updateSourceCells();   // greys the cells a slot's type does not use, names the loaded files
    int  cellForParam(ambient::ParamId id) const;
    int  tableCell_ = -1, impulseCell_ = -1, impulseBCell_ = -1;
    int  textureCell_[ambient::kSlots] = { -1, -1, -1, -1 };   // a Texture... button in every source section
    void buildCells();
    void colourCellsByGroup();
    int  addExtraCell(const juce::String& section, std::unique_ptr<juce::Component> comp, const juce::String& label, int units);
    // Help: the line in the header follows the mouse; the page is the manual by topic.
    void mouseEnter(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override;
    bool keyPressed(const juce::KeyPress&) override;
    int  hoveredParam_ = -1;                              // what the mouse is over, for the help line
    std::map<juce::Component*, int> helpParamOf_;         // every control that has a parameter behind it
public:
    void registerHelp(juce::Component*, ambient::ParamId);   // the modulation strip registers its own
private:
    // Undo, redo and an A/B compare, all in terms of whole parameter snapshots: the instrument
    // has no other state that a knob can destroy, and a snapshot is 320 floats.
    struct Snapshot { std::vector<float> v; juce::String what; };
    std::vector<Snapshot> undo_, redo_;
    Snapshot slotA_, slotB_;
    bool     showingB_ = false;
    Snapshot takeSnapshot(const juce::String& what) const;
    void     restore(const Snapshot&);
    void     pushUndo(const juce::String& what);          // call before changing many parameters at once
    void     doUndo();
    void     doRedo();
    void     swapAB();
    std::unique_ptr<juce::TextButton> undoButton_, redoButton_, abButton_, compactButton_, recallButton_;
    // The two section layers that live inside their own sections rather than in the header:
    // 156 filters and 41 plucks are lists you go looking for, not things you keep on the toolbar.
    // Not owned: the cell owns the component, the editor only needs to read the selection back.
    juce::ComboBox* zPresetBox_ = nullptr;
    juce::ComboBox* strikePresetBox_ = nullptr;
    // Compact: the widest rows wrap into two, so the page is narrower and taller. Everything is
    // still on one page; only the shape changes. Kept in the plugin state.
    bool compact_ = false;
    // The die in a section's title: one click randomises that section, shift-click nudges it.
    std::map<juce::String, juce::Rectangle<int>> diceOf_;
    void     randomiseSection(const juce::String& name, bool subtle);
    void     rebuildLayout();
    struct HelpView : juce::Component, juce::ListBoxModel {
        HelpView(AmbientSynthProcessor&, AmbientSynthEditor&);
        void paint(juce::Graphics&) override;
        void resized() override;
        int  getNumRows() override;
        void paintListBoxItem(int row, juce::Graphics&, int w, int h, bool selected) override;
        void selectedRowsChanged(int row) override;
        void showTopic(int row);
        AmbientSynthProcessor& proc;
        AmbientSynthEditor& owner;
        juce::ListBox topics;
        juce::TextEditor text;
        juce::String parameters;   // the generated last topic: every parameter with its help
        // The pictures: snapshots of the topic's sections, fresh from the panel with its current
        // values, and a live display of the unit -- the same component the page uses, a second copy.
        std::vector<juce::Image> pics;
        std::vector<juce::Rectangle<int>> picRects;
        std::unique_ptr<juce::Component> live;
        // The signal flow, drawn large enough to read: what the routing map in the header was for.
        struct FlowDiagram : juce::Component {
            explicit FlowDiagram(AmbientSynthProcessor& p) : proc(p) { setInterceptsMouseClicks(false, false); }
            void paint(juce::Graphics&) override;
            AmbientSynthProcessor& proc;
        };
        FlowDiagram flow;
        int topic = 0;
    };
    // Snapshots for the manual: a section as it stands on the panel (its tab is switched in for
    // the picture and back again), the modulation strip, the browser page.
    juce::Image snapshotSection(const juce::String& name);
    juce::Image snapshotStrip();
    juce::Image snapshotBrowse();
    // Writes the manual out as pictures and text for Tools/make_manual.py to turn into a PDF.
    // It has to happen here, in a running editor, because the pictures ARE the panel: snapshots
    // of the real sections with their real values, not drawings kept somewhere in step with it.
    void exportManual(const juce::File& dir);
    std::unique_ptr<HelpView> help_;
    std::unique_ptr<juce::TextButton> helpButton_;
    std::unique_ptr<juce::TooltipWindow> tooltips_;

    struct Cell {
        std::unique_ptr<juce::Component> comp;
        std::unique_ptr<juce::Label> label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> slider;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> button;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> combo;
        juce::String baseLabel;
        int units = 1;
        int param = -1;    // ParamId or -1 for extra cells
    };
    struct Section {
        juce::String name;
        std::vector<int> cells;
        juce::Rectangle<int> bounds;
        int maxUnits = 7;
        int wideUnits = 0;          // the natural width, before Compact halves it
        int group = -1;
        bool visible = true;   // false while its tab is not the open one
    };
    struct Group {
        juce::String name;
        juce::Colour colour;
        std::vector<std::vector<juce::String>> rows;   // section names per row
        juce::Rectangle<int> bounds;
        int column = 0;
        std::vector<juce::Component*> displays;        // per row: a display that fills the leftover width, or null
        // A row with no sections in it gets its height from here instead of from them; and if the
        // group is marked stretchy, that row also takes whatever height the taller column has
        // left over, so the two columns end level and the page has no hole in it at any size.
        int  minRowH = 0;
        bool stretch = false;
    };

    // A row of a group whose sections take turns: one page open, the others a tab away. This is
    // what keeps the whole synth on one screen -- the three sources, the two filters, the effect
    // pairs and the conductor's tables are alike enough that seeing one at a time is no loss.
    struct TabRow {
        int group = 0, row = 0;
        std::vector<juce::String> names;                       // one label per page
        std::vector<std::vector<juce::String>> pages;          // section names per page
        std::vector<juce::Component*> displays;                // per page: display for the leftover width, or null
        // Per page, optionally: a section that sits UNDER the display, in the display's column,
        // wrapping to that column's width. Source 1's page keeps the strand bank's ten knobs there
        // rather than on a tab of their own, over a half-height picture of the bank.
        std::vector<juce::String> under;
        int active = 0;
        juce::Rectangle<int> bar;
        std::vector<juce::Rectangle<int>> tabs;
    };
    std::vector<TabRow> tabRows_;
    TabRow* tabRowFor(int group, int row);
    void    setSectionVisible(Section&, bool);
    void    clickTabs(juce::Point<int> contentPos);

    Section* findSection(const juce::String& name);
    int      sectionWidth(const Section&) const;
    int      sectionHeight(const Section&) const;
    void     layoutSection(Section&, int x, int y);

    AmbientSynthProcessor& proc_;
    AmbientLookAndFeel laf_;
    float scale_ = 1.0f;   // window size / design size
    int   designW_ = 1400, designH_ = 820;   // measured from the layout, not guessed
    int   bodyW_ = 0, bodyH_ = 0;
    void  layoutBody();
    std::vector<Cell> cells_;
    std::vector<Section> sections_;
    std::vector<Group> groups_;
    std::map<juce::Component*, int> cellOf_;
    std::unique_ptr<juce::Slider> master_;

    // Live picture of the oscillator: one cycle built from the partial amplitudes the loudest
    // voice is summing right now, plus those partials as a spectrum. It moves because the
    // shimmer and the drift move -- it is the sound, not an illustration of it.
    struct ScopeView : juce::Component, juce::Timer {
        explicit ScopeView(AmbientSynthProcessor& p) : proc(p) { setInterceptsMouseClicks(false, false); startTimerHz(30); }
        void paint(juce::Graphics&) override;
        void timerCallback() override { if (isShowing()) repaint(); }
        AmbientSynthProcessor& proc;
        float amp[ambient::kMaxPartials] = {};   // smoothed towards the engine's values
        int   count = 0;
        bool  mode = false;                      // false = waveform, true = spectrum
    };
    std::unique_ptr<ScopeView> scope_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> masterAttach_;
    // Everything below the header lives in a scrollable content component.
    struct Content : juce::Component {
        std::function<void(juce::Graphics&)> onPaint;
        std::function<void(const juce::MouseEvent&)> onMouse;
        void paint(juce::Graphics& g) override { if (onPaint) onPaint(g); }
        void mouseDown(const juce::MouseEvent& e) override { if (onMouse) onMouse(e); }
    };
    Content content_;
    juce::Viewport viewport_;
    void paintContent(juce::Graphics&);
    std::unique_ptr<juce::TextButton> saveButton_, loadButton_, recButton_, calibButton_, mapButton_, performButton_;
    // Main brings the panel back from any page; VR holds what only a headset needs (calibration,
    // the gesture table) under one button instead of two on the toolbar.
    std::unique_ptr<juce::TextButton> mainButton_, vrButton_;
    // Perform page: the eight macros as large knobs plus the morph, instead of the editor.
    struct PerformView : juce::Component {
        explicit PerformView(AmbientSynthProcessor& p);
        void paint(juce::Graphics&) override;
        void resized() override;
        AmbientSynthProcessor& proc;
        std::vector<std::unique_ptr<juce::Slider>> knobs;
        std::vector<std::unique_ptr<juce::Label>> labels;
        std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> attachments;
        juce::Slider morph;
        juce::ToggleButton morphActive;
        juce::Label morphLabel, aLabel, bLabel;
        // set timeline: record everything that moves, play a set back
        juce::TextButton setRec{ "Record set" }, setPlay{ "Play set..." }, setStop{ "Stop set" };
        juce::Label setInfo;
        std::unique_ptr<juce::FileChooser> chooser;
        void updateSetInfo();
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> morphAttach;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> morphActiveAttach;
    };
    std::unique_ptr<PerformView> perform_;
    bool performing_ = false;
    void setPerforming(bool on);

    // Browse page: filterable preset list plus the preset map (points = presets, drag the
    // cursor to blend between neighbours).
    struct BrowseView : juce::Component, juce::ListBoxModel, juce::Timer {
        explicit BrowseView(AmbientSynthProcessor& p);
        void paint(juce::Graphics&) override;
        void resized() override;
        void timerCallback() override;
        // list
        int  getNumRows() override { return static_cast<int>(filtered.size()); }
        void paintListBoxItem(int row, juce::Graphics& g, int w, int h, bool selected) override;
        void listBoxItemClicked(int row, const juce::MouseEvent&) override;
        void applyFilter();
        // map
        struct MapView : juce::Component {
            explicit MapView(BrowseView& o) : owner(o) {}
            void paint(juce::Graphics&) override;
            void mouseDown(const juce::MouseEvent&) override;
            void mouseDrag(const juce::MouseEvent&) override;
            void mouseMove(const juce::MouseEvent&) override;
            void mouseUp(const juce::MouseEvent&) override;
            int  nearestPreset(juce::Point<float> p, float maxDist) const;
            juce::Point<float> toScreen(float x, float y) const;
            juce::Point<float> toMap(juce::Point<float> s) const;
            BrowseView& owner;
            int hover = -1;
            bool dragging = false;
        };
        // classic column browser (Omnisphere / Absynth style): each column narrows the list
        struct Column : juce::ListBoxModel {
            BrowseView* owner = nullptr;
            juce::String title;
            juce::StringArray items;          // items[0] = "All"
            std::vector<uint32_t> tagBits;    // per item: tag mask (0 for family columns / All)
            std::vector<int> familyIdx;       // per item: family index or -1
            std::set<int> chosen;             // chosen rows (empty = All)
            std::vector<int> counts;          // presets per row, rebuilt when the list grows
            int countsFor = -1;               // the numPresets() the counts were taken at
            void updateCounts();
            juce::ListBox box;
            int  getNumRows() override { return items.size(); }
            void paintListBoxItem(int row, juce::Graphics& g, int w, int h, bool selected) override;
            void listBoxItemClicked(int row, const juce::MouseEvent&) override;
            bool passes(int preset) const;
        };
        Column columns[4];
        void setMode(int m);   // 0 classic columns, 1 map
        int mode = 0;
        juce::TextButton modeClassic{ "Columns" }, modeMap{ "Map" }, star{ "Favourite" };
        juce::ToggleButton onlyFavourites{ "only favourites" };
        // Five thousand presets include some that barely move. Their measurements say so, and
        // this hides them: nothing is deleted, the list simply stops offering them.
        juce::ToggleButton hideDull{ "hide the still ones" };

        AmbientSynthProcessor& proc;
        juce::TextEditor search;
        juce::ComboBox family, sort;
        std::vector<std::unique_ptr<juce::ToggleButton>> tagButtons;
        juce::ListBox list;
        MapView map;
        juce::ToggleButton mapActive;
        juce::Slider radius;
        juce::Label info;
        // route strip (map view): preset routes, play/loop/speed, add the cursor as a point, edit the text
        juce::ComboBox routeBox;
        juce::ToggleButton routePlay, routeLoop;
        juce::Slider routeSpeed;
        juce::TextButton routeAdd{ "+ point" }, routeClear{ "Clear" }, routeEdit{ "Route..." };
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> routePlayAttach, routeLoopAttach;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> routeSpeedAttach;
        juce::Component::SafePointer<juce::TextEditor> routeEditor;
        juce::String routeEditText; bool routeEditOpen = false;
        void showRouteEditor();
        juce::TextButton toA{ "-> A" }, toB{ "-> B" }, load{ "Load" };
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> mapActiveAttach;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> radiusAttach;
        std::vector<int> filtered;
        int selected = -1;
    };
    std::unique_ptr<BrowseView> browse_;
    // The modulation strip along the bottom of the main page, in the shape Pigments uses: a lane
    // of every modulator as a small card with its live curve, a row of tabs, and the full editors
    // for whichever group is open. A modulator you cannot see is a modulator you cannot aim.
    struct ModView : juce::Component, juce::Timer {
        ModView(AmbientSynthProcessor& p, AmbientSynthEditor& o);
        void paint(juce::Graphics&) override;
        void resized() override;
        void timerCallback() override;
        void mouseDown(const juce::MouseEvent&) override;
        void mouseDrag(const juce::MouseEvent&) override;
        void mouseUp(const juce::MouseEvent&) override;
        void mouseMove(const juce::MouseEvent&) override;

        struct Row {   // one LFO or one envelope: its curve and the controls that shape it
            std::vector<std::unique_ptr<juce::Component>> controls;
            std::vector<std::unique_ptr<juce::Label>> labels;
            std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> sliders;
            std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>> combos;
            juce::Rectangle<int> curve;
            juce::String title;
        };
        Row lfos[ambient::kNumLfos];
        Row envs[ambient::kNumModEnvs];

        // A card in the lane: one modulation source, drawn with its own live shape.
        struct Card {
            ambient::ModSource source = ambient::ModSource::None;
            juce::String label;
            juce::Colour colour;
            juce::Rectangle<int> bounds;
            int tab = 0;                 // which tab this card belongs to
            int index = 0;               // LFO or envelope number, for the curve
        };
        std::vector<Card> cards;
        int tab = 0;                     // 0 LFO, 1 envelopes, 2 matrix
        int hoverCard = -1;
        int dragCard = -1;               // the card being dragged onto a knob
        juce::Point<int> dragPos;
        // Editing an envelope's shape: which one, and which of its breakpoints is under the mouse.
        int dragEnv = -1, dragPoint = -1, hoverEnv = -1, hoverPoint = -1;

        void setTab(int t);
        void paintLane(juce::Graphics&);
        void paintCard(juce::Graphics&, const Card&, bool hot);
        void paintLfo(juce::Graphics&, int i);
        void paintEnv(juce::Graphics&, int i);
        void mouseDoubleClick(const juce::MouseEvent&) override;
        // The envelope editor. The shapes were always there in the engine -- sixteen breakpoints,
        // a curve on every segment, a sustain point and a loop -- but nothing could reach them
        // except a preset, so in the panel they were pictures of something you could not touch.
        // One row's geometry, asked for by both the drawing and the mouse so they cannot drift.
        struct EnvGeom { float x0 = 0, w = 1, cy = 0, h = 1, len = 1, depth = 1; };
        EnvGeom envGeom(int env) const;
        int  envAt(juce::Point<int> pos, int* pointOut = nullptr) const;   // -1 if not on a curve
        juce::Point<float> envToXY(int env, float time, float value) const;
        void envFromXY(int env, juce::Point<int> pos, float& time, float& value) const;
        ambient::ModEnv envCopy(int env) const;
        void envCommit(int env, const ambient::ModEnv&, const juce::String& what);
        void envShapeMenu(int env, int point);
        ambient::LfoSpec specOf(int i) const;
        // Where a drag ended: the parameter under the mouse, or none.
        int paramUnder(juce::Point<int> screenPos) const;

        AmbientSynthProcessor& proc;
        AmbientSynthEditor& owner;
        juce::TextButton tabLfo{ "LFO" }, tabEnv{ "ENVELOPES" }, tabMatrix{ "MATRIX" };
        // The matrix page: a table of routes (EditorMatrix.cpp), where a text box used to be.
        std::unique_ptr<edt::RouteTable> table;
        juce::Label matrixInfo, hint;
        juce::Rectangle<int> lane, tabsArea, content;
        ~ModView() override;
        void pullMatrix();
        // Adds a route from a dragged source to a parameter, with a small default depth.
        bool addRoute(ambient::ModSource src, ambient::ParamId target);
    };
    std::unique_ptr<ModView> mod_;
    juce::Viewport modPort_;
    std::unique_ptr<juce::TextButton> modButton_;
public:
    // The cells the modulation strip needs to find a drop target and to mark modulated knobs.
    int cellParamAt(juce::Point<int> screenPos) const;
    juce::Rectangle<int> cellScreenBounds(int cellIndex) const;
private:
    // Displays that live inside the grid, one per section row, filling the room the knobs leave.
    // The grid used to leave that room empty; the displays are what a Pigments-style layout puts
    // there, and each one is drawn from the numbers the engine is using, not from an illustration.

    // The two filters as a frequency response: the state-variable filter in the voice's colour,
    // the z-plane cascade in the accent, and what a note actually meets after both.
    struct FilterView : juce::Component, juce::Timer {
        explicit FilterView(AmbientSynthProcessor& p) : proc(p) { setInterceptsMouseClicks(false, false); startTimerHz(15); }
        void paint(juce::Graphics&) override;
        void timerCallback() override { if (isShowing()) repaint(); }
        AmbientSynthProcessor& proc;
    };
    // One source slot: the wavetable frame, the FM cycle, the clip with its grain window, or the
    // colour of the noise -- whichever the slot is set to.
    struct SourceView : juce::Component, juce::Timer {
        SourceView(AmbientSynthProcessor& p, int s) : proc(p), slot(s) { setInterceptsMouseClicks(false, false); startTimerHz(15); }
        void paint(juce::Graphics&) override;
        void timerCallback() override { if (isShowing()) repaint(); }
        AmbientSynthProcessor& proc;
        int slot;   // 1, 2 or 3
        float amp[ambient::kTablePartials] = {};   // smoothed live amplitudes for the additive picture
    };
    // The conductor's notes as they happen: a piano roll scrolling left, one column per tick, so
    // the cluster brain's choices can be watched rather than inferred from the keyboard strip.
    struct BrainView : juce::Component, juce::Timer {
        explicit BrainView(AmbientSynthProcessor& p) : proc(p) { setInterceptsMouseClicks(false, false); startTimerHz(15); }
        void paint(juce::Graphics&) override;
        void timerCallback() override;
        AmbientSynthProcessor& proc;
        static constexpr int kCols = 240;                 // 16 s at 15 Hz
        std::vector<std::array<bool, 128>> hist = std::vector<std::array<bool, 128>>(kCols);
        int head = 0;
        int lo = 36, hi = 84;                             // the note range in view, widened as notes arrive
    };
    // Three rolls: the conductor's, the second conductor's, and one on the Autoplay tab, where
    // watching the chord travel one voice at a time is the whole point of the mode.
    std::unique_ptr<BrainView> brainView_, brainView2_, brainView3_;
    // The stereo stage: every sounding voice as a dot, left-right by its pan, near-far by its
    // plane, size by its envelope -- the spatial model (concept.md) as a picture, moving.
    struct StageView : juce::Component, juce::Timer {
        explicit StageView(AmbientSynthProcessor& p) : proc(p) { setInterceptsMouseClicks(false, false); startTimerHz(20); }
        void paint(juce::Graphics&) override;
        void timerCallback() override { if (isShowing()) repaint(); }
        AmbientSynthProcessor& proc;
        struct Dot { float x = 0.0f, y = 0.0f, r = 0.0f; int note = -1; bool on = false; };
        Dot dots[ambient::Engine::kMaxVoices];   // smoothed positions, one per voice slot seen
    };
    std::unique_ptr<StageView> stageView_;
    // The Cosmos return's spectrum: what the shifter, the resonator, the vowel and the nebula are
    // handing back, on a log-frequency axis, smoothed the way a meter falls.
    struct CosmosView : juce::Component, juce::Timer {
        explicit CosmosView(AmbientSynthProcessor& p);
        void paint(juce::Graphics&) override;
        void timerCallback() override;
        AmbientSynthProcessor& proc;
        static constexpr int kN = 2048, kBins = 160;
        std::vector<float> re, im, window;
        std::unique_ptr<ambient::Fft> fft;
        float bins[kBins] = {};   // dB per log-spaced bin, smoothed
        bool  silent = true;
    };
    std::unique_ptr<CosmosView> cosmosView_;
    // What the header used to carry was a small spectrum, at 1024 points and 96 bands. The strip
    // along the bottom of the page now does that properly, so this space says the thing a piece
    // of ambient actually has to be judged by: its loudness and, more to the point, how much of
    // its dynamic range is still there. A drone mastered to -9 LUFS has had the movement squeezed
    // out of its reverb tails and cannot get it back.
    struct LoudnessView : juce::Component, juce::Timer {
        explicit LoudnessView(AmbientSynthProcessor& p) : proc(p) { startTimerHz(10); }
        void paint(juce::Graphics&) override;
        void timerCallback() override { if (isShowing()) repaint(); }
        void mouseDown(const juce::MouseEvent&) override;   // click to start measuring again
        AmbientSynthProcessor& proc;
    };
    std::unique_ptr<LoudnessView> outputView_;
    // The Vector's square, with the point in it: three corners are the source slots, the fourth
    // is all three together. Drag the point; it is the one control here whose value is a place.
    struct VectorView : juce::Component, juce::Timer {
        explicit VectorView(AmbientSynthProcessor& p) : proc(p) { startTimerHz(20); }
        void paint(juce::Graphics&) override;
        void timerCallback() override { if (isShowing()) repaint(); }
        void mouseDown(const juce::MouseEvent& e) override { drag(e); }
        void mouseDrag(const juce::MouseEvent& e) override { drag(e); }
        void drag(const juce::MouseEvent&);
        juce::Rectangle<float> square() const;
        AmbientSynthProcessor& proc;
        static constexpr int kTrail = 240;      // where the wander has been, at 20 Hz: twelve seconds
        float tx[kTrail] = {}, ty[kTrail] = {};
        int   head = 0, filled = 0;
    };
    std::unique_ptr<VectorView> vectorView_;
    // The wide strip along the bottom of the left column: the same signal as the header's little
    // meter, but with room to say something. A 16384-sample window is 2.9 Hz wide at 48 kHz, so
    // the partials of a low drone are separate lines rather than a hump, and the filter's own
    // response is drawn over them -- what the filter is doing to what is actually there.
    struct SpectrumView : juce::Component, juce::Timer {
        explicit SpectrumView(AmbientSynthProcessor& p);
        void paint(juce::Graphics&) override;
        void timerCallback() override;
        void mouseMove(const juce::MouseEvent&) override;
        void mouseExit(const juce::MouseEvent&) override;
        AmbientSynthProcessor& proc;
        static constexpr int kN = ambient::kOutTapLen;   // FFT length: the whole tap
        static constexpr int kBands = 480;               // one band per two or three pixels
        static constexpr float kLoHz = 20.0f, kHiHz = 16000.0f;
        // The axis runs a little above zero so the filter, drawn on the same decibels, has
        // somewhere to put a resonance: a flat response is the 0 dB line, not the ceiling.
        static constexpr float kFloorDb = -84.0f, kTopDb = 6.0f;
        static constexpr int kLabelGutter = 26;   // room at the left for the decibel numbers
        std::vector<float> re, im, window;
        std::unique_ptr<ambient::Fft> fft;
        std::vector<float> band, hold;                   // live bands, and a slowly falling trace
        float peakDb = -96.0f;
        int   hoverX = -1;                               // read the frequency under the pointer
    };
    std::unique_ptr<SpectrumView> spectrumView_;
    // The amplitude envelope as a curve, with the loudest voice's level on it.
    struct EnvView : juce::Component, juce::Timer {
        explicit EnvView(AmbientSynthProcessor& p) : proc(p) { setInterceptsMouseClicks(false, false); startTimerHz(20); }
        void paint(juce::Graphics&) override;
        void timerCallback() override { if (isShowing()) repaint(); }
        AmbientSynthProcessor& proc;
    };
    std::unique_ptr<EnvView> envView_;
    std::unique_ptr<FilterView> filterView_;
    std::unique_ptr<SourceView> source1View_, source2View_, source3View_, source4View_;
    std::unique_ptr<juce::TextButton> browseButton_;
    void setPage(int page);   // 0 edit, 1 perform, 2 browse
    void showMappingEditor();
    juce::Component::SafePointer<juce::TextEditor> mapEditor_;
    juce::String mapText_;
    bool mapOpen_ = false;
    std::unique_ptr<juce::ComboBox> soundBox_;
    juce::ComboBox* cosmosBox_ = nullptr;   // not owned: its cell in the Cosmos section owns it
    juce::ComboBox* morphABox_ = nullptr;   // owned by their cells
    juce::ComboBox* morphBBox_ = nullptr;
    std::unique_ptr<juce::FileChooser> chooser_;
    juce::Rectangle<int> header_, helpLine_, keys_;
    bool sounding_[128] = {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AmbientSynthEditor)
};
