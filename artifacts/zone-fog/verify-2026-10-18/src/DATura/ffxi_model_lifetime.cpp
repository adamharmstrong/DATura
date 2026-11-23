#include "stdafx.h"
#include "ffxi_model_lifetime.h"

#include "noesis_rapi.h"

namespace FFXIModelLifetime
{
OwnedModel::~OwnedModel()
{
    Reset();
}

OwnedModel::OwnedModel(OwnedModel&& other) noexcept
    : model_(other.model_), rapi_(other.rapi_)
{
    other.model_ = nullptr;
    other.rapi_ = nullptr;
}

OwnedModel& OwnedModel::operator=(OwnedModel&& other) noexcept
{
    if (this == &other)
        return *this;

    Reset();
    model_ = other.model_;
    rapi_ = other.rapi_;
    other.model_ = nullptr;
    other.rapi_ = nullptr;
    return *this;
}

noesisModel_t* OwnedModel::Model() const noexcept
{
    return model_;
}

noeRAPI_t* OwnedModel::ParserContext() const noexcept
{
    return rapi_;
}

OwnedModel::operator bool() const noexcept
{
    return model_ != nullptr;
}

void OwnedModel::Adopt(noesisModel_t* model, noeRAPI_t* rapi) noexcept
{
    if (model_ == model && rapi_ == rapi)
        return;

    Reset();
    model_ = model;
    rapi_ = rapi;
}

void OwnedModel::AttachModel(noesisModel_t* model) noexcept
{
    if (model_ == model)
        return;

    if (model_)
        model_->ReleaseD3DBuffers();
    model_ = model;
}

void OwnedModel::Reset() noexcept
{
    Release(model_, rapi_);
}

void ReleaseParserContext(noeRAPI_t*& rapi)
{
    if (rapi)
    {
        delete rapi;
        rapi = nullptr;
    }
}

void Release(noesisModel_t*& model, noeRAPI_t*& rapi)
{
    if (model)
    {
        model->ReleaseD3DBuffers();
        model = nullptr;
    }
    ReleaseParserContext(rapi);
}
}
