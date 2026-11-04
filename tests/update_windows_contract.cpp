#include "SuiteUpdater.h"
#include <iostream>
#include <chrono>

namespace suite_update
{
// A friend exercises the production transport and hash pipeline against the
// existing release without offering a downgrade in the application UI.
struct ServiceContract
{
    static bool Running(Service &s)
    {
        return s.running.load();
    }

    static bool Download(Service &s, Release release)
    {
        s.received.store(0);
        s.Set(Phase::Downloading, "Checking the published update package.");
        return s.Start([&s, release] { s.Download(release); });
    }

    static bool HasCredential()
    {
        auto value = Service::Credential();
        bool found = !value.empty();
        if (found)
            SecureZeroMemory(value.data(), value.size());
        return found;
    }

    static void Forget()
    {
        CredDeleteW(Service::CredentialName, CRED_TYPE_GENERIC, 0);
    }
};
} // namespace suite_update

void Require(bool condition, const char *message)
{
    if (!condition)
        throw std::runtime_error(message);
}

void Finish(suite_update::Service &service)
{
    auto until = std::chrono::steady_clock::now() + std::chrono::seconds(180);
    while (suite_update::ServiceContract::Running(service))
    {
        if (std::chrono::steady_clock::now() >= until)
        {
            service.Cancel();
            throw std::runtime_error("The production GitHub transport timed out.");
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    auto view = service.View();
    if (view->phase == suite_update::Phase::Error)
        throw std::runtime_error(view->message);
}

int main(int argc, char **argv)
{
    using namespace suite_update;
    const bool anonymous = argc == 2 && std::string_view(argv[1]) == "--anonymous";
    const bool staleCredential = argc == 2 && std::string_view(argv[1]) == "--stale-credential";
    std::string token(TokenLimit + 1, 0);
    DWORD count = GetEnvironmentVariableA("SUITE_UPDATE_TEST_TOKEN", token.data(), DWORD(token.size()));
    if (!anonymous && !staleCredential && (!count || count >= token.size()))
    {
        std::cerr << "CI release-read token is required for the private-repository transport check.\n";
        return 1;
    }
    token.resize(anonymous || staleCredential ? 0 : count);
    if (staleCredential)
        token = "intentionally-invalid-public-release-test-token";
    if (ServiceContract::HasCredential())
    {
        SecureZeroMemory(token.data(), token.size());
        std::cerr << "Refusing to replace an existing release credential.\n";
        return 1;
    }
    fs::path root =
        fs::temp_directory_path() / (L"SuiteNativeUpdate-" + std::to_wstring(GetCurrentProcessId()));
    int result = 0;
    try
    {
        fs::create_directories(root);
        Service service;
        service.Init(root);
        Require(!service.bundled.empty() && service.bundled.front().version == suite_brand::Version,
                "The embedded current changelog is missing.");
        if (!anonymous)
        {
            if (!service.StoreCredential(token))
                throw std::runtime_error(service.stateError);
            SecureZeroMemory(token.data(), token.size());
            token.clear();
            Require(ServiceContract::HasCredential(), "The release credential was not readable.");
        }
        Require(service.Check(), "The production release check did not start.");
        Finish(service);
        auto view = service.View();
        Require(!view->releases.empty(), "The real GitHub release list is empty.");
        const auto release = view->releases.front();
        Require(release.Installable(), "The published release has no verified portable update package.");
        Require(ServiceContract::Download(service, release),
                "The production package transfer did not start.");
        Finish(service);
        Require(service.View()->phase == Phase::Ready && service.received.load() == release.size,
                "The complete package was not verified.");
        bool package = false;
        for (const auto &entry : fs::recursive_directory_iterator(root))
            if (entry.path().filename() == L"package.zip")
                package = fs::file_size(entry.path()) == release.size;
        Require(package, "The verified package was not staged.");
        if (!anonymous)
            Require(service.StoreCredential({}) && !ServiceContract::HasCredential(),
                    "The CI credential was not removed.");
        Require(!ServiceContract::HasCredential(), "Public access created an unexpected credential.");
        std::cout
            << "Windows production WinHTTP, " << (anonymous ? "anonymous public releases" : staleCredential ? "public releases with a revoked credential" : "authenticated private releases")
            << ", safe CDN redirects, streaming SHA-256, embedded changelog and Credential Manager cleanup: PASS\n";
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        result = 1;
    }
    if (!token.empty())
        SecureZeroMemory(token.data(), token.size());
    ServiceContract::Forget();
    std::error_code error;
    fs::remove_all(root, error);
    return result;
}
