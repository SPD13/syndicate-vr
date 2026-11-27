// Syndicate Wars VR viewer: OpenXR: session, views, swapchains, controller input.
#pragma once

#include "common.h"

/******************************************************************************/
// OpenXR

#define XR_CHECK(x)                                                    \
    do {                                                               \
        XrResult r_ = (x);                                             \
        if (XR_FAILED(r_)) {                                           \
            logf("%s failed: %d (line %d)", #x, (int)r_, __LINE__);   \
            return false;                                              \
        }                                                              \
    } while (0)

class XrApp {
public:
    // whether the Steam Frame's controller profile is in use
    bool frame_ext_ = false;
    bool init()
    {
        std::vector<const char *> exts = {XR_KHR_OPENGL_ENABLE_EXTENSION_NAME};
        // the Steam Frame's controllers have their own profile, behind a
        // Valve extension: asked for when the runtime has it
        {
            uint32_t n_ext = 0;
            xrEnumerateInstanceExtensionProperties(nullptr, 0, &n_ext, nullptr);
            std::vector<XrExtensionProperties> props(n_ext, {XR_TYPE_EXTENSION_PROPERTIES});
            xrEnumerateInstanceExtensionProperties(nullptr, n_ext, &n_ext, props.data());
            for (const XrExtensionProperties &p : props)
                if (!strcmp(p.extensionName, "XR_VALVE_frame_controller_interaction"))
                    frame_ext_ = true;
            if (frame_ext_)
                exts.push_back("XR_VALVE_frame_controller_interaction");
        }
        XrInstanceCreateInfo ci{XR_TYPE_INSTANCE_CREATE_INFO};
        strcpy(ci.applicationInfo.applicationName, "SyndWarsVR Spectator");
        ci.applicationInfo.applicationVersion = 1;
        strcpy(ci.applicationInfo.engineName, "syndwarsfx");
        ci.applicationInfo.apiVersion = XR_API_VERSION_1_0;
        ci.enabledExtensionCount = (uint32_t)exts.size();
        ci.enabledExtensionNames = exts.data();
        XR_CHECK(xrCreateInstance(&ci, &instance_));

        XrInstanceProperties ip{XR_TYPE_INSTANCE_PROPERTIES};
        xrGetInstanceProperties(instance_, &ip);
        logf("OpenXR runtime: %s", ip.runtimeName);

        XrSystemGetInfo si{XR_TYPE_SYSTEM_GET_INFO};
        si.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
        XR_CHECK(xrGetSystem(instance_, &si, &system_));

        PFN_xrGetOpenGLGraphicsRequirementsKHR getreq = nullptr;
        XR_CHECK(xrGetInstanceProcAddr(instance_, "xrGetOpenGLGraphicsRequirementsKHR",
                                       (PFN_xrVoidFunction *)&getreq));
        XrGraphicsRequirementsOpenGLKHR req{XR_TYPE_GRAPHICS_REQUIREMENTS_OPENGL_KHR};
        XR_CHECK(getreq(instance_, system_, &req));

        uint32_t n = 0;
        XR_CHECK(xrEnumerateViewConfigurationViews(instance_, system_,
                 XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, 0, &n, nullptr));
        view_cfg_.resize(n, {XR_TYPE_VIEW_CONFIGURATION_VIEW});
        XR_CHECK(xrEnumerateViewConfigurationViews(instance_, system_,
                 XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, n, &n, view_cfg_.data()));
        return true;
    }

    bool create_session()
    {
        XrGraphicsBindingOpenGLWin32KHR gb{XR_TYPE_GRAPHICS_BINDING_OPENGL_WIN32_KHR};
        gb.hDC = wglGetCurrentDC();
        gb.hGLRC = wglGetCurrentContext();
        XrSessionCreateInfo sci{XR_TYPE_SESSION_CREATE_INFO};
        sci.next = &gb;
        sci.systemId = system_;
        XR_CHECK(xrCreateSession(instance_, &sci, &session_));

        XrReferenceSpaceCreateInfo rs{XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
        rs.poseInReferenceSpace.orientation.w = 1;
        rs.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_STAGE;
        if (XR_FAILED(xrCreateReferenceSpace(session_, &rs, &space_))) {
            rs.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
            XR_CHECK(xrCreateReferenceSpace(session_, &rs, &space_));
        }
        rs.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW;
        XR_CHECK(xrCreateReferenceSpace(session_, &rs, &view_space_));

        uint32_t nf = 0;
        XR_CHECK(xrEnumerateSwapchainFormats(session_, 0, &nf, nullptr));
        std::vector<int64_t> formats(nf);
        XR_CHECK(xrEnumerateSwapchainFormats(session_, nf, &nf, formats.data()));
        int64_t fmt = formats[0];
        for (int64_t f : formats)
            if (f == GL_SRGB8_ALPHA8) { fmt = f; break; }
        srgb_ = (fmt == GL_SRGB8_ALPHA8);

        for (auto &vc : view_cfg_) {
            Eye e;
            e.w = vc.recommendedImageRectWidth;
            e.h = vc.recommendedImageRectHeight;
            XrSwapchainCreateInfo sc{XR_TYPE_SWAPCHAIN_CREATE_INFO};
            sc.usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT;
            sc.format = fmt;
            sc.sampleCount = 1;
            sc.width = e.w;
            sc.height = e.h;
            sc.faceCount = 1;
            sc.arraySize = 1;
            sc.mipCount = 1;
            XR_CHECK(xrCreateSwapchain(session_, &sc, &e.swapchain));
            uint32_t ni = 0;
            XR_CHECK(xrEnumerateSwapchainImages(e.swapchain, 0, &ni, nullptr));
            e.images.resize(ni, {XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_KHR});
            XR_CHECK(xrEnumerateSwapchainImages(e.swapchain, ni, &ni,
                     (XrSwapchainImageBaseHeader *)e.images.data()));
            glGenFramebuffers(1, &e.fbo);
            glGenRenderbuffers(1, &e.depth);
            glBindRenderbuffer(GL_RENDERBUFFER, e.depth);
            glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, e.w, e.h);
            eyes_.push_back(e);
        }
        logf("XR session created, %zu views %dx%d, %s swapchain", eyes_.size(),
             eyes_.empty() ? 0 : eyes_[0].w, eyes_.empty() ? 0 : eyes_[0].h,
             srgb_ ? "sRGB" : "linear");
        return create_actions();
    }

    bool create_actions()
    {
        XrActionSetCreateInfo asi{XR_TYPE_ACTION_SET_CREATE_INFO};
        strcpy(asi.actionSetName, "spectator");
        strcpy(asi.localizedActionSetName, "Spectator");
        XR_CHECK(xrCreateActionSet(instance_, &asi, &action_set_));

        auto make = [&](XrAction &a, const char *name, const char *loc, XrActionType t) {
            XrActionCreateInfo ai{XR_TYPE_ACTION_CREATE_INFO};
            ai.actionType = t;
            strcpy(ai.actionName, name);
            strcpy(ai.localizedActionName, loc);
            return xrCreateAction(action_set_, &ai, &a);
        };
        XR_CHECK(make(act_pan_, "pan", "Pan map", XR_ACTION_TYPE_VECTOR2F_INPUT));
        XR_CHECK(make(act_zoom_, "zoom_rotate", "Zoom and rotate", XR_ACTION_TYPE_VECTOR2F_INPUT));
        XR_CHECK(make(act_follow_, "toggle_follow", "Toggle follow camera", XR_ACTION_TYPE_BOOLEAN_INPUT));
        XR_CHECK(make(act_recenter_, "recenter", "Recenter table", XR_ACTION_TYPE_BOOLEAN_INPUT));
        XR_CHECK(make(act_look_, "toggle_look", "Switch classic / upgraded look", XR_ACTION_TYPE_BOOLEAN_INPUT));
        XR_CHECK(make(act_world_, "toggle_world", "Whole world / table only", XR_ACTION_TYPE_BOOLEAN_INPUT));
        XR_CHECK(make(act_fog_, "toggle_fog", "Fog of war on / off", XR_ACTION_TYPE_BOOLEAN_INPUT));
        XR_CHECK(make(act_center_, "center_agent", "Centre the view on the selected agent",
                      XR_ACTION_TYPE_BOOLEAN_INPUT));
        XR_CHECK(make(act_aim_l_, "aim_left", "Left pointer", XR_ACTION_TYPE_POSE_INPUT));
        XR_CHECK(make(act_aim_r_, "aim_right", "Right pointer", XR_ACTION_TYPE_POSE_INPUT));
        XR_CHECK(make(act_trig_l_, "command_left", "Shoot", XR_ACTION_TYPE_BOOLEAN_INPUT));
        XR_CHECK(make(act_trig_r_, "command_right", "Select or move", XR_ACTION_TYPE_BOOLEAN_INPUT));
        XR_CHECK(make(act_wep_next_, "weapon_next", "Next weapon", XR_ACTION_TYPE_BOOLEAN_INPUT));
        XR_CHECK(make(act_wep_prev_, "weapon_prev", "Previous weapon", XR_ACTION_TYPE_BOOLEAN_INPUT));
        XR_CHECK(make(act_alt_r_, "hud_right_click", "Right click on the HUD", XR_ACTION_TYPE_BOOLEAN_INPUT));
        XR_CHECK(make(act_alt_l_, "left_right_click", "Right click with the left hand", XR_ACTION_TYPE_BOOLEAN_INPUT));

        auto path = [&](const char *s) {
            XrPath p;
            xrStringToPath(instance_, s, &p);
            return p;
        };
        struct Profile {
            const char *name;
            const char *follow;
            const char *trigger; // per hand, after /user/hand/<side>
            const char *wep_prev, *wep_next;
            const char *center; // centre the view on the selected agent
        };
        const Profile profiles[] = {
            {"/interaction_profiles/valve/index_controller", "/user/hand/right/input/a/click",
             "/input/trigger/click", "/user/hand/left/input/a/click", "/user/hand/left/input/b/click",
             "/user/hand/left/input/trackpad/force"},
            {"/interaction_profiles/oculus/touch_controller", "/user/hand/right/input/a/click",
             "/input/trigger/value", "/user/hand/left/input/x/click", "/user/hand/left/input/y/click",
             "/user/hand/left/input/menu/click"},
        };
        for (const auto &pr : profiles) {
            std::vector<XrActionSuggestedBinding> b = {
                {act_pan_, path("/user/hand/left/input/thumbstick")},
                {act_zoom_, path("/user/hand/right/input/thumbstick")},
                {act_follow_, path(pr.follow)},
                {act_recenter_, path("/user/hand/left/input/thumbstick/click")},
                {act_look_, path("/user/hand/right/input/b/click")},
                {act_world_, path("/user/hand/right/input/thumbstick/click")},
                // the grips: each hand's right button (the fog is G on
                // the desktop with these controllers)
                {act_alt_l_, path("/user/hand/left/input/squeeze/value")},
                {act_alt_r_, path("/user/hand/right/input/squeeze/value")},
                {act_aim_l_, path("/user/hand/left/input/aim/pose")},
                {act_aim_r_, path("/user/hand/right/input/aim/pose")},
                {act_trig_l_, path((std::string("/user/hand/left") + pr.trigger).c_str())},
                {act_trig_r_, path((std::string("/user/hand/right") + pr.trigger).c_str())},
                {act_wep_prev_, path(pr.wep_prev)},
                {act_wep_next_, path(pr.wep_next)},
                {act_center_, path(pr.center)},
            };
            XrInteractionProfileSuggestedBinding sb{XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};
            sb.interactionProfile = path(pr.name);
            sb.suggestedBindings = b.data();
            sb.countSuggestedBindings = (uint32_t)b.size();
            XrResult r = xrSuggestInteractionProfileBindings(instance_, &sb);
            if (XR_FAILED(r))
                logf("bindings for %s rejected: %d", pr.name, (int)r);
        }
        if (frame_ext_) {
            // Steam Frame: R2 (the bottom trigger) selects and clicks (the
            // left click), R1 (the top one, the bumper) is the right click;
            // L2 shoots; the left d-pad picks the weapon
            std::vector<XrActionSuggestedBinding> b = {
                {act_pan_, path("/user/hand/left/input/thumbstick")},
                {act_zoom_, path("/user/hand/right/input/thumbstick")},
                {act_follow_, path("/user/hand/right/input/a/click")},
                {act_recenter_, path("/user/hand/left/input/thumbstick/click")},
                {act_look_, path("/user/hand/right/input/b/click")},
                {act_world_, path("/user/hand/right/input/thumbstick/click")},
                {act_fog_, path("/user/hand/right/input/squeeze/value")},
                {act_aim_l_, path("/user/hand/left/input/aim/pose")},
                {act_aim_r_, path("/user/hand/right/input/aim/pose")},
                {act_trig_l_, path("/user/hand/left/input/trigger/value")},
                {act_trig_r_, path("/user/hand/right/input/trigger/value")},
                {act_alt_r_, path("/user/hand/right/input/bumper/click")},
                {act_alt_l_, path("/user/hand/left/input/bumper/click")},
                {act_center_, path("/user/hand/left/input/squeeze/value")},
                {act_wep_prev_, path("/user/hand/left/input/dpad_left/click")},
                {act_wep_next_, path("/user/hand/left/input/dpad_right/click")},
            };
            XrInteractionProfileSuggestedBinding sb{XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};
            sb.interactionProfile = path("/interaction_profiles/valve/frame_controller_valve");
            sb.suggestedBindings = b.data();
            sb.countSuggestedBindings = (uint32_t)b.size();
            XrResult r = xrSuggestInteractionProfileBindings(instance_, &sb);
            logf("Steam Frame controller bindings %s", XR_FAILED(r) ? "rejected" : "set");
        }
        XrSessionActionSetsAttachInfo at{XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO};
        at.countActionSets = 1;
        at.actionSets = &action_set_;
        XR_CHECK(xrAttachSessionActionSets(session_, &at));
        XrActionSpaceCreateInfo asci{XR_TYPE_ACTION_SPACE_CREATE_INFO};
        asci.poseInActionSpace.orientation.w = 1;
        asci.action = act_aim_l_;
        XR_CHECK(xrCreateActionSpace(session_, &asci, &aim_space_[0]));
        asci.action = act_aim_r_;
        XR_CHECK(xrCreateActionSpace(session_, &asci, &aim_space_[1]));
        return true;
    }

    // Returns false when the app should quit.
    bool poll_events()
    {
        XrEventDataBuffer ev{XR_TYPE_EVENT_DATA_BUFFER};
        while (xrPollEvent(instance_, &ev) == XR_SUCCESS) {
            if (ev.type == XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING) {
                // the runtime's recentring (SteamVR "reset view"): a new
                // forward for the play area
                logf("XR reference space changes (recentred)");
                space_changed_ = true;
            }
            if (ev.type == XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED) {
                auto *sc = (XrEventDataSessionStateChanged *)&ev;
                state_ = sc->state;
                logf("XR session state %d", (int)state_);
                if (state_ == XR_SESSION_STATE_FOCUSED && !was_focused_) {
                    was_focused_ = true;
                    just_focused_ = true;
                }
                if (state_ == XR_SESSION_STATE_READY) {
                    XrSessionBeginInfo bi{XR_TYPE_SESSION_BEGIN_INFO};
                    bi.primaryViewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
                    xrBeginSession(session_, &bi);
                    running_ = true;
                } else if (state_ == XR_SESSION_STATE_STOPPING) {
                    xrEndSession(session_);
                    running_ = false;
                } else if (state_ == XR_SESSION_STATE_EXITING ||
                           state_ == XR_SESSION_STATE_LOSS_PENDING) {
                    return false;
                }
            } else if (ev.type == XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING) {
                return false;
            }
            ev = {XR_TYPE_EVENT_DATA_BUFFER};
        }
        return true;
    }

    struct Input {
        glm::vec2 pan{0}, zoom{0};
        bool follow_pressed = false, recenter_pressed = false, look_pressed = false, world_pressed = false, fog_pressed = false;
        bool center_pressed = false; // centre the view on the selected agent
        bool trig_pressed[2] = {false, false}; // left, right
        bool trig_held[2] = {false, false};
        bool alt_pressed = false, alt_held = false; // the right click on the HUD
        // the left hand's top trigger (Steam Frame: L1)
        bool alt_l_pressed = false, alt_l_held = false;
        bool wep_next = false, wep_prev = false;
        bool aim_valid[2] = {false, false};
        glm::vec3 aim_pos[2], aim_dir[2];
    };

    Input read_input()
    {
        Input in;
        if (!running_)
            return in;
        XrActiveActionSet as{action_set_, XR_NULL_PATH};
        XrActionsSyncInfo sync{XR_TYPE_ACTIONS_SYNC_INFO};
        sync.countActiveActionSets = 1;
        sync.activeActionSets = &as;
        if (XR_FAILED(xrSyncActions(session_, &sync)))
            return in;
        XrActionStateGetInfo gi{XR_TYPE_ACTION_STATE_GET_INFO};
        XrActionStateVector2f v{XR_TYPE_ACTION_STATE_VECTOR2F};
        gi.action = act_pan_;
        if (XR_SUCCEEDED(xrGetActionStateVector2f(session_, &gi, &v)) && v.isActive)
            in.pan = {v.currentState.x, v.currentState.y};
        gi.action = act_zoom_;
        v = {XR_TYPE_ACTION_STATE_VECTOR2F};
        if (XR_SUCCEEDED(xrGetActionStateVector2f(session_, &gi, &v)) && v.isActive)
            in.zoom = {v.currentState.x, v.currentState.y};
        XrActionStateBoolean b{XR_TYPE_ACTION_STATE_BOOLEAN};
        gi.action = act_follow_;
        if (XR_SUCCEEDED(xrGetActionStateBoolean(session_, &gi, &b)))
            in.follow_pressed = b.isActive && b.changedSinceLastSync && b.currentState;
        b = {XR_TYPE_ACTION_STATE_BOOLEAN};
        gi.action = act_recenter_;
        if (XR_SUCCEEDED(xrGetActionStateBoolean(session_, &gi, &b)))
            in.recenter_pressed = b.isActive && b.changedSinceLastSync && b.currentState;
        b = {XR_TYPE_ACTION_STATE_BOOLEAN};
        gi.action = act_look_;
        if (XR_SUCCEEDED(xrGetActionStateBoolean(session_, &gi, &b)))
            in.look_pressed = b.isActive && b.changedSinceLastSync && b.currentState;
        b = {XR_TYPE_ACTION_STATE_BOOLEAN};
        gi.action = act_world_;
        if (XR_SUCCEEDED(xrGetActionStateBoolean(session_, &gi, &b)))
            in.world_pressed = b.isActive && b.changedSinceLastSync && b.currentState;
        b = {XR_TYPE_ACTION_STATE_BOOLEAN};
        gi.action = act_fog_;
        if (XR_SUCCEEDED(xrGetActionStateBoolean(session_, &gi, &b)))
            in.fog_pressed = b.isActive && b.changedSinceLastSync && b.currentState;
        b = {XR_TYPE_ACTION_STATE_BOOLEAN};
        gi.action = act_center_;
        if (XR_SUCCEEDED(xrGetActionStateBoolean(session_, &gi, &b)))
            in.center_pressed = b.isActive && b.changedSinceLastSync && b.currentState;
        auto pressed = [&](XrAction a) {
            XrActionStateGetInfo g{XR_TYPE_ACTION_STATE_GET_INFO};
            g.action = a;
            XrActionStateBoolean st{XR_TYPE_ACTION_STATE_BOOLEAN};
            return XR_SUCCEEDED(xrGetActionStateBoolean(session_, &g, &st)) && st.isActive &&
                   st.changedSinceLastSync && st.currentState;
        };
        in.trig_pressed[0] = pressed(act_trig_l_);
        in.trig_pressed[1] = pressed(act_trig_r_);
        auto held = [&](XrAction a) {
            XrActionStateGetInfo g{XR_TYPE_ACTION_STATE_GET_INFO};
            g.action = a;
            XrActionStateBoolean st{XR_TYPE_ACTION_STATE_BOOLEAN};
            return XR_SUCCEEDED(xrGetActionStateBoolean(session_, &g, &st)) && st.isActive && st.currentState;
        };
        in.trig_held[0] = held(act_trig_l_);
        in.trig_held[1] = held(act_trig_r_);
        in.alt_pressed = pressed(act_alt_r_);
        in.alt_held = held(act_alt_r_);
        in.alt_l_pressed = pressed(act_alt_l_);
        in.alt_l_held = held(act_alt_l_);
        in.wep_next = pressed(act_wep_next_);
        in.wep_prev = pressed(act_wep_prev_);
        for (int h = 0; h < 2 && last_time_ != 0; h++) {
            XrSpaceLocation loc{XR_TYPE_SPACE_LOCATION};
            if (XR_FAILED(xrLocateSpace(aim_space_[h], space_, last_time_, &loc)))
                continue;
            if (!(loc.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT) ||
                !(loc.locationFlags & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT))
                continue;
            glm::quat q(loc.pose.orientation.w, loc.pose.orientation.x, loc.pose.orientation.y,
                        loc.pose.orientation.z);
            in.aim_pos[h] = {loc.pose.position.x, loc.pose.position.y, loc.pose.position.z};
            in.aim_dir[h] = q * glm::vec3(0, 0, -1);
            in.aim_valid[h] = true;
        }
        return in;
    }

    // Pose of the head in the stage space, if tracked.
    bool head_pose(XrTime t, glm::vec3 &pos, glm::quat &ori)
    {
        XrSpaceLocation loc{XR_TYPE_SPACE_LOCATION};
        if (XR_FAILED(xrLocateSpace(view_space_, space_, t, &loc)))
            return false;
        if (!(loc.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT) ||
            !(loc.locationFlags & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT))
            return false;
        pos = {loc.pose.position.x, loc.pose.position.y, loc.pose.position.z};
        ori = glm::quat(loc.pose.orientation.w, loc.pose.orientation.x, loc.pose.orientation.y,
                        loc.pose.orientation.z);
        return true;
    }

    template <typename DrawFn>
    void frame(DrawFn draw, XrTime &out_time)
    {
        out_time = 0;
        if (!running_) {
            Sleep(10);
            return;
        }
        XrFrameWaitInfo wi{XR_TYPE_FRAME_WAIT_INFO};
        XrFrameState fs{XR_TYPE_FRAME_STATE};
        if (XR_FAILED(xrWaitFrame(session_, &wi, &fs)))
            return;
        XrFrameBeginInfo bi{XR_TYPE_FRAME_BEGIN_INFO};
        if (XR_FAILED(xrBeginFrame(session_, &bi)))
            return;
        out_time = fs.predictedDisplayTime;
        last_time_ = fs.predictedDisplayTime;

        std::vector<XrCompositionLayerProjectionView> pviews;
        XrCompositionLayerProjection layer{XR_TYPE_COMPOSITION_LAYER_PROJECTION};
        std::vector<XrCompositionLayerBaseHeader *> layers;
        if (fs.shouldRender) {
            uint32_t nv = (uint32_t)eyes_.size();
            std::vector<XrView> views(nv, {XR_TYPE_VIEW});
            XrViewLocateInfo li{XR_TYPE_VIEW_LOCATE_INFO};
            li.viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
            li.displayTime = fs.predictedDisplayTime;
            li.space = space_;
            XrViewState vs{XR_TYPE_VIEW_STATE};
            if (XR_SUCCEEDED(xrLocateViews(session_, &li, &vs, nv, &nv, views.data())) &&
                (vs.viewStateFlags & XR_VIEW_STATE_POSITION_VALID_BIT)) {
                pviews.resize(nv, {XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW});
                for (uint32_t i = 0; i < nv; i++) {
                    Eye &e = eyes_[i];
                    uint32_t idx = 0;
                    XrSwapchainImageAcquireInfo ai{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
                    xrAcquireSwapchainImage(e.swapchain, &ai, &idx);
                    XrSwapchainImageWaitInfo wsi{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
                    wsi.timeout = XR_INFINITE_DURATION;
                    xrWaitSwapchainImage(e.swapchain, &wsi);

                    glBindFramebuffer(GL_FRAMEBUFFER, e.fbo);
                    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                                           e.images[idx].image, 0);
                    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                                              GL_RENDERBUFFER, e.depth);
                    glViewport(0, 0, e.w, e.h);
                    if (srgb_)
                        glEnable(GL_FRAMEBUFFER_SRGB);
                    glm::mat4 proj = projection(views[i].fov, 0.02f, 100.0f);
                    glm::mat4 view = glm::inverse(pose_matrix(views[i].pose));
                    draw(proj * view, srgb_);
                    glDisable(GL_FRAMEBUFFER_SRGB);
                    if (i == 0)
                        last_left_ = {e.fbo, e.w, e.h};
                    glBindFramebuffer(GL_FRAMEBUFFER, 0);

                    XrSwapchainImageReleaseInfo ri{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
                    xrReleaseSwapchainImage(e.swapchain, &ri);

                    pviews[i].pose = views[i].pose;
                    pviews[i].fov = views[i].fov;
                    pviews[i].subImage.swapchain = e.swapchain;
                    pviews[i].subImage.imageRect.offset = {0, 0};
                    pviews[i].subImage.imageRect.extent = {e.w, e.h};
                }
                layer.space = space_;
                layer.viewCount = nv;
                layer.views = pviews.data();
                layers.push_back((XrCompositionLayerBaseHeader *)&layer);
            }
        }
        XrFrameEndInfo ei{XR_TYPE_FRAME_END_INFO};
        ei.displayTime = fs.predictedDisplayTime;
        ei.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
        ei.layerCount = (uint32_t)layers.size();
        ei.layers = layers.data();
        xrEndFrame(session_, &ei);
    }

    struct Mirror {
        GLuint fbo = 0;
        int w = 0, h = 0;
    };
    Mirror last_left() const { return last_left_; }
    bool running() const { return running_; }
    // True once, when the headset is first put on and the app gets focus.
    bool take_space_changed()
    {
        bool r = space_changed_;
        space_changed_ = false;
        return r;
    }
    bool take_just_focused()
    {
        bool r = just_focused_;
        just_focused_ = false;
        return r;
    }

private:
    static glm::mat4 pose_matrix(const XrPosef &p)
    {
        glm::quat q(p.orientation.w, p.orientation.x, p.orientation.y, p.orientation.z);
        glm::mat4 m = glm::mat4_cast(q);
        m[3] = glm::vec4(p.position.x, p.position.y, p.position.z, 1.0f);
        return m;
    }

    static glm::mat4 projection(const XrFovf &fov, float n, float f)
    {
        float l = std::tan(fov.angleLeft), r = std::tan(fov.angleRight);
        float d = std::tan(fov.angleDown), u = std::tan(fov.angleUp);
        glm::mat4 m(0.0f);
        m[0][0] = 2.0f / (r - l);
        m[1][1] = 2.0f / (u - d);
        m[2][0] = (r + l) / (r - l);
        m[2][1] = (u + d) / (u - d);
        m[2][2] = -(f + n) / (f - n);
        m[2][3] = -1.0f;
        m[3][2] = -2.0f * f * n / (f - n);
        return m;
    }

    struct Eye {
        XrSwapchain swapchain = XR_NULL_HANDLE;
        int32_t w = 0, h = 0;
        std::vector<XrSwapchainImageOpenGLKHR> images;
        GLuint fbo = 0, depth = 0;
    };

    XrInstance instance_ = XR_NULL_HANDLE;
    XrSystemId system_ = XR_NULL_SYSTEM_ID;
    XrSession session_ = XR_NULL_HANDLE;
    XrSpace space_ = XR_NULL_HANDLE, view_space_ = XR_NULL_HANDLE;
    std::vector<XrViewConfigurationView> view_cfg_;
    std::vector<Eye> eyes_;
    XrSessionState state_ = XR_SESSION_STATE_UNKNOWN;
    bool running_ = false, srgb_ = false;
    bool was_focused_ = false, just_focused_ = false, space_changed_ = false;
    XrActionSet action_set_ = XR_NULL_HANDLE;
    XrAction act_pan_ = XR_NULL_HANDLE, act_zoom_ = XR_NULL_HANDLE, act_follow_ = XR_NULL_HANDLE,
             act_recenter_ = XR_NULL_HANDLE, act_look_ = XR_NULL_HANDLE, act_aim_l_ = XR_NULL_HANDLE,
             act_aim_r_ = XR_NULL_HANDLE, act_trig_l_ = XR_NULL_HANDLE, act_trig_r_ = XR_NULL_HANDLE,
             act_alt_r_ = XR_NULL_HANDLE, act_alt_l_ = XR_NULL_HANDLE,
             act_wep_next_ = XR_NULL_HANDLE, act_wep_prev_ = XR_NULL_HANDLE, act_world_ = XR_NULL_HANDLE, act_fog_ = XR_NULL_HANDLE,
             act_center_ = XR_NULL_HANDLE;
    XrSpace aim_space_[2] = {XR_NULL_HANDLE, XR_NULL_HANDLE};
    XrTime last_time_ = 0;
    Mirror last_left_;
};
