//
//  JsSkinDocument.cpp
//  MusicPlayer
//
//  Created by henry_xiao on 2022/12/22.
//

#include "JsSkinDocument.hpp"
#include "objects/JsLibObject.hpp"
#include "strings/CommonString.hpp"
#include "SkinJsAPI.hpp"
#include "JsUIObject.hpp"
#include "../SkinFactory.h"
#include "../SkinWnd.h"


JsValue jsValuePrototypeDocument;

JsSkinDocument::JsSkinDocument(CSkinWnd *skinWnd) : JsObjectX(SJT_DOCUMENT, jsValuePrototypeDocument), _skinWnd(skinWnd) {
}

bool JsSkinDocument::onSetValue(VMContext *ctx, const StringView &name, const JsValue &value) {
    if (name.equal(SS_HEIGHT)) {
        auto h = (int32_t)ctx->runtime->toNumber(ctx, value);
        if (h >= 10 && h <= 1920) {
            CRect rc;
            if (_skinWnd->getWindowRect(&rc)) {
                _skinWnd->moveWindow(rc.left, rc.top, rc.width(), h);
            }
        }
    } else if (name.equal(SS_ON_COMMAND)) {
        _skinWnd->m_onCommandListener = value;
    } else if (name.equal(SS_ON_MOUSE_MOVE)) {
        _skinWnd->m_onMouseMoveListener = value;
    } else if (name.equal(SS_ON_SIZE)) {
        _skinWnd->m_onSizeListener = value;
    } else if (name.equal(SS_ON_ACTIVATE)) {
        _skinWnd->m_onActivateListener = value;
    } else if (name.equal(SS_ON_DESTORY)) {
        _skinWnd->m_onDestoryListener = value;
    } else if (name.equal(SS_ON_MOUSE_ACTIVATE)) {
        _skinWnd->m_onMouseActivateListener = value;
    }

    return false;
}

JsValue JsSkinDocument::onGetValue(VMContext *ctx, const StringView &name) {
    if (name.equal(SS_HEIGHT)) {
        CRect rc;
        if (_skinWnd->getWindowRect(&rc)) {
            return makeJsValueInt32(rc.height());
        }
    } else if (name.equal(SS_ON_COMMAND)) {
        return _skinWnd->m_onCommandListener;
    } else if (name.equal(SS_ON_MOUSE_MOVE)) {
        return _skinWnd->m_onMouseMoveListener;
    } else if (name.equal(SS_ON_SIZE)) {
        return _skinWnd->m_onSizeListener;
    } else if (name.equal(SS_ON_ACTIVATE)) {
        return _skinWnd->m_onActivateListener;
    } else if (name.equal(SS_ON_DESTORY)) {
        return _skinWnd->m_onDestoryListener;
    } else if (name.equal(SS_ON_MOUSE_ACTIVATE)) {
        return _skinWnd->m_onMouseActivateListener;
    }

    return jsValueEmpty;
}

void JsSkinDocument::onEnumAllProperties(VMContext *ctx, VecStringViews &names, VecJsValues &values) {
    CRect rc;
    if (_skinWnd->getWindowRect(&rc)) {
        names.push_back(SS_HEIGHT);
        values.push_back(makeJsValueInt32(rc.height()));
    }

    names.push_back(SS_ON_COMMAND);
    values.push_back(_skinWnd->m_onCommandListener);

    names.push_back(SS_ON_MOUSE_MOVE);
    values.push_back(_skinWnd->m_onMouseMoveListener);

    names.push_back(SS_ON_SIZE);
    values.push_back(_skinWnd->m_onSizeListener);

    names.push_back(SS_ON_ACTIVATE);
    values.push_back(_skinWnd->m_onActivateListener);

    names.push_back(SS_ON_DESTORY);
    values.push_back(_skinWnd->m_onDestoryListener);

    names.push_back(SS_ON_MOUSE_ACTIVATE);
    values.push_back(_skinWnd->m_onMouseActivateListener);
}

void JsSkinDocument::markReferIdx(VMRuntime *rt) {
    rt->markReferIdx(_skinWnd->m_onMouseMoveListener);
    rt->markReferIdx(_skinWnd->m_onSizeListener);
    rt->markReferIdx(_skinWnd->m_onActivateListener);
    rt->markReferIdx(_skinWnd->m_onDestoryListener);
    rt->markReferIdx(_skinWnd->m_onCommandListener);
    rt->markReferIdx(_skinWnd->m_onMouseActivateListener);
}

CSkinWnd *documentGetSkinWnd(VMContext *ctx, const JsValue &thiz) {
    if (thiz.type != JDT_OBJ_X) {
        ctx->throwExceptionFormatJsValue(JE_TYPE_ERROR, "document's this must be JsDocument type.", thiz);
        return nullptr;
    }

    auto obj = (JsObjectX *)ctx->runtime->getObject(thiz);
    if (obj->extraType() != SJT_DOCUMENT) {
        ctx->throwExceptionFormatJsValue(JE_TYPE_ERROR, "document's this must be JsDocument type(2).", thiz);
        return nullptr;
    }

    return ((JsSkinDocument *)obj)->skinWnd();
}

int getIdFromArg0(CSkinWnd *skinWnd, VMContext *ctx, const Arguments &args) {
    int id = ID_INVALID;
    auto idVal = args.getAt(0);
    if (idVal.type == JDT_INT32) {
        id = getJsValueInt32(idVal);
    } else {
        auto str = ctx->runtime->toStringView(ctx, idVal);
        id = skinWnd->getSkinFactory()->getIDByName(str.toString().c_str());
    }

    if (id == ID_INVALID) {
        ctx->throwExceptionFormatJsValue(JE_TYPE_ERROR, "Invalid UIObject Id: %.*s", idVal);
        return ID_INVALID;
    }

    return id;
}

void document_getElementById(VMContext *ctx, const JsValue &thiz, const Arguments &args) {
    auto skinWnd = documentGetSkinWnd(ctx, thiz);
    if (skinWnd == nullptr) {
        return;
    }

    int id = getIdFromArg0(skinWnd, ctx, args);

    CUIObject *obj = skinWnd->getUIObjectById(id);
    if (obj == nullptr) {
        ctx->throwExceptionFormatJsValue(JE_TYPE_ERROR, "UIObject cannot be found by Id: %.*s", args.getAt(0));
        return;
    }

    ctx->retValue = obj->getJsObject(ctx);
}

void document_getCommandID(VMContext *ctx, const JsValue &thiz, const Arguments &args) {
    auto skinWnd = documentGetSkinWnd(ctx, thiz);
    if (skinWnd == nullptr) {
        return;
    }

    int id = getIdFromArg0(skinWnd, ctx, args);
    ctx->retValue = makeJsValueInt32(id);
}

void document_moveWindow(VMContext *ctx, const JsValue &thiz, const Arguments &args) {
    auto skinWnd = documentGetSkinWnd(ctx, thiz);
    if (skinWnd == nullptr) {
        return;
    }

    ctx->retValue = jsValueUndefined;

    if (args.count == 2) {
        auto x = args.getIntAt(ctx, 0);
        auto y = args.getIntAt(ctx, 1);
        skinWnd->setWindowPosSafely(x, y);
        return;
    } else if (args.count == 4) {
        auto x = args.getIntAt(ctx, 0);
        auto y = args.getIntAt(ctx, 1);
        auto w = args.getIntAt(ctx, 2);
        auto h = args.getIntAt(ctx, 3);
        if (w >= 10 && h >= 10) {
            skinWnd->moveWindowSafely(x, y, w, h);
            return;
        }
    }

    ctx->throwException(JE_RANGE_ERROR, "Invalid window size/position.");
}

void document_postCommand(VMContext *ctx, const JsValue &thiz, const Arguments &args) {
    auto skinWnd = documentGetSkinWnd(ctx, thiz);
    if (skinWnd == nullptr) {
        return;
    }

    ctx->retValue = jsValueUndefined;

    int id = getIdFromArg0(skinWnd, ctx, args);
    if (id != ID_INVALID) {
        skinWnd->postCustomCommandMsg(id);
    }
}

void document_startAnimation(VMContext *ctx, const JsValue &thiz, const Arguments &args) {
    auto skinWnd = documentGetSkinWnd(ctx, thiz);
    if (skinWnd == nullptr) {
        return;
    }

    ctx->retValue = jsValueUndefined;

    int id = getIdFromArg0(skinWnd, ctx, args);
    if (id != ID_INVALID) {
        skinWnd->startAnimation(id);
    }
}

void document_stopAnimation(VMContext *ctx, const JsValue &thiz, const Arguments &args) {
    auto skinWnd = documentGetSkinWnd(ctx, thiz);
    if (skinWnd == nullptr) {
        return;
    }

    ctx->retValue = jsValueUndefined;

    int id = getIdFromArg0(skinWnd, ctx, args);
    if (id != ID_INVALID) {
        skinWnd->stopAnimation(id);
    }
}

static JsLibProperty documentPrototypeFunctions[] = {
    { "name", nullptr, "document" },
    { "length", nullptr, nullptr, jsValueLength1Property },
    { "getElementById", document_getElementById },
    { "getCommandID", document_getCommandID },
    { "moveWindow", document_moveWindow },
    { "postCommand", document_postCommand },
    { "startAnimation", document_startAnimation },
    { "stopAnimation", document_stopAnimation },
};

void registerDocument(VMRuntimeCommon *rt) {
    auto prototypeObj = new JsLibObject(rt, documentPrototypeFunctions, CountOf(documentPrototypeFunctions));
    jsValuePrototypeDocument = rt->pushObject(prototypeObj);
}
