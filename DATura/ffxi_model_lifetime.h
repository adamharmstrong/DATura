#pragma once

struct noesisModel_t;
class noeRAPI_t;

// Owns the teardown order for a model and the parser context that backs it.
// GPU buffers must be released before the context is destroyed.
namespace FFXIModelLifetime
{
    class OwnedModel final
    {
    public:
        OwnedModel() noexcept = default;
        ~OwnedModel();

        OwnedModel(const OwnedModel&) = delete;
        OwnedModel& operator=(const OwnedModel&) = delete;
        OwnedModel(OwnedModel&& other) noexcept;
        OwnedModel& operator=(OwnedModel&& other) noexcept;

        noesisModel_t* Model() const noexcept;
        noeRAPI_t* ParserContext() const noexcept;
        explicit operator bool() const noexcept;

        // Takes responsibility for both pointers. The model's GPU buffers are
        // always released before the parser context that backs it is deleted.
        void Adopt(noesisModel_t* model, noeRAPI_t* rapi) noexcept;

        // Associates a parser-created model with an already-owned context.
        // This supports failure-safe loading: own the context first, then
        // attach the model only after parsing succeeds.
        void AttachModel(noesisModel_t* model) noexcept;
        void Reset() noexcept;

    private:
        noesisModel_t* model_ = nullptr;
        noeRAPI_t* rapi_ = nullptr;
    };

    void ReleaseParserContext(noeRAPI_t*& rapi);
    void Release(noesisModel_t*& model, noeRAPI_t*& rapi);
}
