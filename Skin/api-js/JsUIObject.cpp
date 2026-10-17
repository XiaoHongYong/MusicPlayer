//
//  JsUIObject.cpp
//  MusicPlayer
//
//  Created by henry_xiao on 2022/12/22.
//

#include "JsUIObject.hpp"
#include "interpreter/VirtualMachineTypes.hpp"
#include "objects/JsLibObject.hpp"
#include "strings/CommonString.hpp"
#include "objects/JsObjectX.hpp"
#include "SkinJsAPI.hpp"
#include "../SkinFactory.h"
#include "../SkinWnd.h"


JsValue jsValuePrototypeUIObject;

const StringView SS_VISIBLE = makeCommonString("visible");
const StringView SS_FORM_WIDTH = makeCommonString("formWidth");

JsUIObject::JsUIObject(CUIObject *obj) : JsObjectX(SJT_UIOBJECT, jsValuePrototypeUIObject), _obj(obj) {
}

bool JsUIObject::onSetValue(VMContext *ctx, const StringView &name, const JsValue &value) {
    if (name.equal(SS_VISIBLE)) {
        _obj->setVisible(ctx->runtime->testTrue(value), true);
    } else if (name.equal(SS_FORM_WIDTH)) {
        auto str = ctx->runtime->toStringView(ctx, value);
        _obj->setProperty(SZ_PN_WIDTH, str.toString().c_str());
    } else if (name.equal(SS_ON_MOUSE_ENTER)) {
        _obj->m_onMouseEnterListener = value;
    } else if (name.equal(SS_ON_MOUSE_LEAVE)) {
        _obj->m_onMouseLeaveListener = value;
    } else if (name.equal(SS_ON_KILL_FOCUS)) {
        _obj->m_onKillFocusListener = value;
    }

    return false;
}

JsValue JsUIObject::onGetValue(VMContext *ctx, const StringView &name) {
    if (name.equal(SS_VISIBLE)) {
        return makeJsValueBool(_obj->isVisible());
    } else if (name.equal(SS_FORM_WIDTH)) {
        return ctx->runtime->pushString(_obj->m_formWidth.getFormula());
    } else if (name.equal(SS_ID)) {
        return makeJsValueInt32(_obj->getID());
    } else if (name.equal(SS_NAME)) {
        return ctx->runtime->pushString(_obj->m_strName.c_str());
    } else if (name.equal(SS_ON_MOUSE_ENTER)) {
        return _obj->m_onMouseEnterListener;
    } else if (name.equal(SS_ON_MOUSE_LEAVE)) {
        return _obj->m_onMouseLeaveListener;
    } else if (name.equal(SS_ON_KILL_FOCUS)) {
        return _obj->m_onKillFocusListener;
    }

    return jsValueEmpty;
}

void JsUIObject::onEnumAllProperties(VMContext *ctx, VecStringViews &names, VecJsValues &values) {
    names.push_back(SS_VISIBLE);
    values.push_back(makeJsValueBool(_obj->isVisible()));

    names.push_back(SS_FORM_WIDTH);
    values.push_back(ctx->runtime->pushString(_obj->m_formWidth.getFormula()));

    names.push_back(SS_ID);
    values.push_back(makeJsValueInt32(_obj->getID()));

    names.push_back(SS_NAME);
    values.push_back(ctx->runtime->pushString(_obj->m_strName.c_str()));

    names.push_back(SS_ON_MOUSE_ENTER);
    values.push_back(_obj->m_onMouseEnterListener);

    names.push_back(SS_ON_MOUSE_LEAVE);
    values.push_back(_obj->m_onMouseLeaveListener);

    names.push_back(SS_ON_KILL_FOCUS);
    values.push_back(_obj->m_onKillFocusListener);
}

CUIObject *convertToUIObject(VMContext *ctx, const JsValue &thiz, const Arguments &args) {
    if (thiz.type != JDT_OBJ_X) {
        ctx->throwExceptionFormatJsValue(JE_TYPE_ERROR, "UIObject's this must be UIObject type.", thiz);
        return nullptr;
    }

    auto obj = (JsObjectX *)ctx->runtime->getObject(thiz);
    if (obj->extraType() != SJT_UIOBJECT) {
        ctx->throwExceptionFormatJsValue(JE_TYPE_ERROR, "UIObject's this must be UIObject type(2).", thiz);
        return nullptr;
    }

    return ((JsUIObject *)obj)->uiobject();
}

void uiobject_setVisible(VMContext *ctx, const JsValue &thiz, const Arguments &args) {
    auto obj = convertToUIObject(ctx, thiz, args);
    if (obj) {
        obj->setVisible(args.getBoolAt(ctx, 0));
    }
}

void uiobject_setFocus(VMContext *ctx, const JsValue &thiz, const Arguments &args) {
    auto obj = convertToUIObject(ctx, thiz, args);
    if (obj) {
        obj->setFocus();
    }
}

static JsLibProperty uiobjectPrototypeFunctions[] = {
    { "name", nullptr, "UIObject" },
    { "length", nullptr, nullptr, jsValueLength1Property },
    { "setFocus", uiobject_setFocus },
};

void registerUIObject(VMRuntimeCommon *rt) {
    auto prototypeObj = new JsLibObject(rt, uiobjectPrototypeFunctions, CountOf(uiobjectPrototypeFunctions));
    jsValuePrototypeUIObject = rt->pushObject(prototypeObj);
}
