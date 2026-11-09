#include <juce_events/juce_events.h>
#include <juce_core/juce_core.h>

class ConsoleRunner final : public juce::UnitTestRunner
{
    void logMessage (const juce::String& message) override
    {
        std::printf ("%s\n", message.toRawUTF8());
        std::fflush (stdout);
    }
};

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    ConsoleRunner runner;
    runner.setAssertOnFailure (false);
    runner.setPassesAreLogged (false);

    if (argc > 1)
        runner.runTestsInCategory (argv[1]);
    else
        runner.runAllTests();

    int failures = 0, passes = 0;
    for (int i = 0; i < runner.getNumResults(); ++i)
    {
        failures += runner.getResult (i)->failures;
        passes += runner.getResult (i)->passes;
    }

    std::printf ("\n%d checks passed, %d failed\n", passes, failures);
    return failures == 0 ? 0 : 1;
}
