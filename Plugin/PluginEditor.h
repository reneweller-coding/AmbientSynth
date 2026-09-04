#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
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
    juce::LookAndFeel_V4 laf_;
    std::vector<Cell> cells_;
    std::vector<Section> sections_;
    std::vector<Group> groups_;
    std::map<juce::Component*, int> cellOf_;
    std::unique_ptr<juce::Slider> master_;
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
