#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include "AmbientLookAndFeel.h"
#include "ambient/Modulation.h"
#include <vector>
#include <memory>
#include <map>
#include <set>

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
    void chooseSourceFile(bool wavetable);
    void chooseImpulseFile();
    void updateSourceCells();   // greys the cells a slot's type does not use, names the loaded files
    int  cellForParam(ambient::ParamId id) const;
    int  tableCell_ = -1, textureCell_ = -1, impulseCell_ = -1;
    void buildCells();
    void colourCellsByGroup();
    int  addExtraCell(const juce::String& section, std::unique_ptr<juce::Component> comp, const juce::String& label, int units);
    void paintRoutingMap(juce::Graphics&, juce::Rectangle<int> area);

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
        int group = -1;
    };
    struct Group {
        juce::String name;
        juce::Colour colour;
        std::vector<std::vector<juce::String>> rows;   // section names per row
        juce::Rectangle<int> bounds;
        int column = 0;
    };

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
        void paint(juce::Graphics& g) override { if (onPaint) onPaint(g); }
    };
    Content content_;
    juce::Viewport viewport_;
    void paintContent(juce::Graphics&);
    std::unique_ptr<juce::TextButton> saveButton_, loadButton_, recButton_, calibButton_, mapButton_, performButton_;
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
    // Modulation page: the eight LFOs and six envelopes, each as a curve you can see moving,
    // with its knobs beside it -- and the matrix underneath. A modulator you cannot see is a
    // modulator you cannot aim, which is the whole reason the shapes are drawn here at all.
    struct ModView : juce::Component, juce::Timer {
        explicit ModView(AmbientSynthProcessor& p);
        void paint(juce::Graphics&) override;
        void resized() override;
        void timerCallback() override;

        // One LFO or one envelope: a curve plus the controls that shape it.
        struct Row {
            std::vector<std::unique_ptr<juce::Component>> controls;
            std::vector<std::unique_ptr<juce::Label>> labels;
            std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> sliders;
            std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>> combos;
            juce::Rectangle<int> curve;      // where the shape is drawn
            juce::String title;
        };
        Row lfos[ambient::kNumLfos];
        Row envs[ambient::kNumModEnvs];

        void paintLfo(juce::Graphics&, int i);
        void paintEnv(juce::Graphics&, int i);
        ambient::LfoSpec specOf(int i) const;

        AmbientSynthProcessor& proc;
        // The matrix as text, the way the gesture mappings already are: thirty-two rows of
        // controls would be a page of their own, and the text form is what the preset stores.
        juce::TextEditor matrixText;
        juce::TextButton applyMatrix{ "Apply" }, clearMatrix{ "Clear" };
        juce::Label matrixInfo, hint;
        void pushMatrix();
        void pullMatrix();
    };
    std::unique_ptr<ModView> mod_;
    // The page is taller than most windows -- eight LFOs, six envelopes and the matrix -- so it
    // scrolls rather than shrinking its rows until the curves are unreadable.
    juce::Viewport modPort_;
    std::unique_ptr<juce::TextButton> modButton_;
    std::unique_ptr<juce::TextButton> browseButton_;
    void setPage(int page);   // 0 edit, 1 perform, 2 browse
    void showMappingEditor();
    juce::Component::SafePointer<juce::TextEditor> mapEditor_;
    juce::String mapText_;
    bool mapOpen_ = false;
    std::unique_ptr<juce::ComboBox> soundBox_, cosmosBox_;
    juce::ComboBox* morphABox_ = nullptr;   // owned by their cells
    juce::ComboBox* morphBBox_ = nullptr;
    std::unique_ptr<juce::FileChooser> chooser_;
    juce::Rectangle<int> header_, routing_, keys_;
    bool sounding_[128] = {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AmbientSynthEditor)
};
