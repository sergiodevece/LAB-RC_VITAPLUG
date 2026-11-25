#include <juce_core/juce_core.h>
#if LABRC_TEST_GUI
#include <juce_gui_basics/juce_gui_basics.h>
#endif
#include <exception>
#include <iostream>

namespace
{
class ConsoleRunner final : public juce::UnitTestRunner
{
    void logMessage(const juce::String& message) override
    {
        std::cout << message << std::endl;
    }
};
}

int main(int argc, char** argv)
{
    juce::StringArray categories;
    for(int i = 1; i < argc; ++i)
    {
        const juce::String argument(argv[i]);
        if(argument == "--category" && i + 1 < argc)
            categories.add(argv[++i]);
        else if(argument == "--benchmark")
            categories.add("Benchmark");
        else if(argument == "--all")
            categories.addArray({"DSP", "Processor", "UI"});
        else if(argument == "--help")
        {
            std::cout << "labrc_tests [--category DSP|Processor|UI] [--benchmark] [--all]\n";
            return 0;
        }
        else
        {
            std::cerr << "Unknown or incomplete argument: " << argument << '\n';
            return 2;
        }
    }
    if(categories.isEmpty())
    {
        categories.add("DSP");
       #if LABRC_TEST_GUI
        categories.addArray({"Processor", "UI"});
       #endif
    }
    categories.removeDuplicates(false);
    juce::Array<juce::UnitTest*> selected;
    for(const auto& category : categories)
    {
        const auto tests = juce::UnitTest::getTestsInCategory(category);
        if(tests.isEmpty())
        {
            std::cerr << "No registered tests in category: " << category << '\n';
            return 3;
        }
        selected.addArray(tests);
    }
    try
    {
       #if LABRC_TEST_GUI
        juce::ScopedJuceInitialiser_GUI gui;
       #endif
        ConsoleRunner runner;
        runner.setAssertOnFailure(false);
        runner.setPassesAreLogged(false);
        runner.runTests(selected, 0x4c41425243LL);
        int passes = 0, failures = 0;
        for(int i = 0; i < runner.getNumResults(); ++i)
            if(const auto* result = runner.getResult(i))
            {
                passes += result->passes;
                failures += result->failures;
            }
        std::cout << "LAB-RC: " << runner.getNumResults() << " sections, " << passes
                  << " checks passed, " << failures << " failed\n";
        return failures == 0 && runner.getNumResults() > 0 ? 0 : 1;
    }
    catch(const std::exception& error)
    {
        std::cerr << "Test exception: " << error.what() << '\n';
        return 1;
    }
    catch(...)
    {
        std::cerr << "Unknown test exception\n";
        return 1;
    }
}
