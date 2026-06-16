#pragma once

#include <juce_core/juce_core.h>

namespace daw
{
/** A minimal, GUI-free most-recently-used file list. juce's RecentlyOpenedFilesList
    lives in juce_gui_extra, which daw_core must not pull in, so this is the headless
    equivalent: newest-first, de-duplicated, capped, with newline string persistence.
    The UI renders it into a File > Open Recent menu. */
class RecentFilesList
{
public:
    void setMaxItems (int n) { maxItems = juce::jmax (1, n); }

    /** Moves `f` to the front (de-duplicating), trimming to the cap. */
    void add (const juce::File& f)
    {
        items.removeAllInstancesOf (f);
        items.insert (0, f);
        while (items.size() > maxItems)
            items.removeLast();
    }

    void clear() { items.clear(); }

    int size() const { return items.size(); }

    /** Newest-first list of files. */
    const juce::Array<juce::File>& files() const { return items; }

    /** Newline-joined absolute paths, for storing in app settings. */
    juce::String toString() const
    {
        juce::StringArray paths;
        for (const auto& f : items)
            paths.add (f.getFullPathName());
        return paths.joinIntoString ("\n");
    }

    /** Restores from a string produced by toString(); silently drops blank lines. */
    void restoreFromString (const juce::String& s)
    {
        items.clear();
        juce::StringArray paths;
        paths.addLines (s);

        // toString() writes newest-first; add() prepends, so replay oldest-first to end
        // up newest-first again.
        for (int i = paths.size(); --i >= 0;)
            if (paths[i].isNotEmpty())
                add (juce::File (paths[i]));
    }

private:
    juce::Array<juce::File> items;
    int maxItems = 16;
};
} // namespace daw
