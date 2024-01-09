// #define COW_PATCH_FRAMERATE
// #define COW_PATCH_FRAMERATE_SLEEP
#include "include.cpp"

////////////////////////////////////////////////////////////////////////////////
// mini world //////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

// OrbitCamera always points at the world origin; offers a third person perspective of the scene

struct OrbitCamera {
    double distance;
    double theta; // rotation about y axis
    double phi; // rotation about x axis
    double _angle_of_view;
};

mat4 orbit_camera_get_C(OrbitCamera *orbit) {
    return M4_RotationAboutYAxis(orbit->theta) * M4_RotationAboutXAxis(orbit->phi) * M4_Translation(0, 0, orbit->distance);
}

void orbit_camera_move(OrbitCamera *orbit) {
    if ((globals._mouse_owner == COW_MOUSE_OWNER_NONE) && globals.mouse_left_held) {
        orbit->theta -= globals.mouse_change_in_position_NDC.x;
        // phi is clamped to prevent camera from "passing over the north or south poles"
        orbit->phi = CLAMP(orbit->phi + globals.mouse_change_in_position_NDC.y, RAD(-90), RAD(90));
    }

    orbit->distance = orbit->distance + globals.mouse_wheel_offset;
}

// --------------------------------------------------

// FPSCamera presents a first-person perspective of a human in the scene; controllable by keyboard movements

struct FPSCamera {
    vec3 origin;
    double theta;
    double phi;
    double _angle_of_view;
};

mat4 fps_camera_get_C(FPSCamera *human) {
    return M4_Translation(human->origin) * M4_RotationAboutYAxis(human->theta) * M4_RotationAboutXAxis(human->phi);
}

bool jump_up = true;
bool override = false;

void fps_camera_move(FPSCamera *human) {
    // hold W key to walking forward
    if (globals.key_held['w']) {
        (human->origin).z -= 0.5*cos(human->theta);
        (human->origin).x -= 0.5*sin(human->theta);
    }
    // hold shift to sprint
    if (globals.key_shift_held) {
        (human->origin).z -= 1.0*cos(human->theta);
        (human->origin).x -= 1.0*sin(human->theta);
    }
    // hold S key to walk backward
    if (globals.key_held['s']) {
        (human->origin).z += 0.5*cos(human->theta);
        (human->origin).x += 0.5*sin(human->theta);
    }
    // hold A, D key to strafe left and right
    if (globals.key_held['a']) {
        (human->origin).z -= 0.5*cos(human->theta + RAD(90));
        (human->origin).x -= 0.5*sin(human->theta + RAD(90));
    }
    if (globals.key_held['d']) {
        (human->origin).z += 0.5*cos(human->theta + RAD(90));
        (human->origin).x += 0.5*sin(human->theta + RAD(90));
    }
    // hold spacebar to jump
    if (globals.key_held[COW_KEY_SPACE]) {
        if (jump_up) {
            (human->origin).y += 0.5;
            if ((human->origin).y >= 20) {
                jump_up = false;
            }
        } else {
            (human->origin).y -= 0.5;
            if ((human->origin).y <= 10) {
                jump_up = true;
            }
        }
        override = false;
    } else if (!globals.key_held[COW_KEY_SPACE] && !override && (human->origin).y != 10) {
        (human->origin).y -= 0.5;
    }
    // hold F, G to fly up and down
    if (globals.key_held['f']) {
        override = true;
        (human->origin).y += 0.5;
    }
    if (globals.key_held['g'] && (human->origin).y >= 10) {
        override = true;
        (human->origin).y -= 0.5;
    }

    if (window_is_pointer_locked()) {
        human->theta -= globals.mouse_change_in_position_NDC.x;
        // phi is clamped to prevent being able to "look backward through your legs"
        human->phi = CLAMP(human->phi + globals.mouse_change_in_position_NDC.y, RAD(-90), RAD(90));
    }
}

// --------------------------------------------------

// TrackingCamera is a camera fixed in space that tracks and looks only at the plane camera.

struct TrackingCamera {
    vec3 origin;
    vec3 *target;
    double _angle_of_view;
};

mat4 tracking_camera_get_C(TrackingCamera *track) {
    vec3 z = normalized(track->origin - *(track->target));
    vec3 x = normalized(cross(V3(0, 1, 0), z));
    vec3 y = cross(z, x);
    mat4 position = xyzo2mat4(x, y, z, track->origin);
    return position;
}

// --------------------------------------------------

// ArbitraryCamera represents the first-person perspective of a plane flying in the air; controllable by keyboard movements

struct ArbitraryCamera {
    // note: using a mat4's to represent rotation is maybe not the best
    //       but it sure is convenient                                 
    vec3 origin;
    mat4 R;
    double _angle_of_view;
};

mat4 arbitrary_camera_get_C(ArbitraryCamera *plane) {
    return M4_Translation(plane->origin) * plane->R; // TODO
}

void arbitrary_camera_move(ArbitraryCamera *plane) {
    // hold W key to pitch down
    if (globals.key_held['w']) {
        plane->R *= M4_RotationAboutXAxis(RAD(-1));
    }
    // hold S key to pitch up
    if (globals.key_held['s']) {
        plane->R *= M4_RotationAboutXAxis(RAD(1));
    }
    // hold A to yaw left
    if (globals.key_held['a']) {
        plane->R *= M4_RotationAboutYAxis(RAD(1));
    }
    // hold D to yaw right
    if (globals.key_held['d']) {
        plane->R *= M4_RotationAboutYAxis(RAD(-1));
    }
    // hold J to roll left
    if (globals.key_held['j']) {
        plane->R *= M4_RotationAboutZAxis(RAD(1));
    }
    // hold L to roll right
    if (globals.key_held['l']) {
        plane->R *= M4_RotationAboutZAxis(RAD(-1));
    }

    plane->origin += transformVector(plane->R, V3(0, 0, -1));
}

// --------------------------------------------------

void draw_box_with_fake_shadows(mat4 PV, mat4 M, vec3 color) {
    library.soups.box.draw(PV * M, color, 3);
    { // shadows
        Soup3D shadow_box = library.soups.box;
        shadow_box.primitive = SOUP_QUADS;

        // goes from world coordinates of the object to world coordinates of the shadow
        mat4 M_FakeShadow = {
            1, 0, 0, 0,
            0, 0, 0, 1,
            0, 0, 1, 0,
            0, 0, 0, 1,
        };

        vec4 shadow_color = V4(0, 0, 0, .2);

        {
            glEnable(GL_CULL_FACE);
            glCullFace(GL_BACK);
            shadow_box.draw(PV * M_FakeShadow * M, shadow_color);
            glDisable(GL_CULL_FACE);
        }
    }
}

void draw_world(mat4 P, mat4 V) {
    mat4 PV = P * V;

    { // little patch of grass
        double r = 100;
        vec3 verts[] = { { -r, 0, -r }, { -r, 0,  r }, {  r, 0,  r }, {  r, 0, -r } };
        soup_draw(PV, SOUP_QUADS, 4, verts, NULL, .7 * monokai.green);
    }
    { // trees
        draw_box_with_fake_shadows(PV, M4_Translation(40, 10, 25) * M4_Scaling(3, 10, 3), monokai.brown);
        draw_box_with_fake_shadows(PV, M4_Translation(40, 20, 25) * M4_Scaling(10, 10, 10), AVG(monokai.green, monokai.yellow));
        draw_box_with_fake_shadows(PV, M4_Translation(60, 15, 50) * M4_Scaling(3, 15, 3) * M4_RotationAboutYAxis(RAD(30)), monokai.brown);
        draw_box_with_fake_shadows(PV, M4_Translation(60, 30, 50) * M4_Scaling(10, 10, 10) * M4_RotationAboutYAxis(RAD(30)), monokai.green);
    }
    { // outer box
        vec3 vertex_colors[] = {
            .5 * monokai.black,  .5 * monokai.black,  .5 * monokai.blue,   .5 * monokai.purple,
            .5 * monokai.orange, .5 * monokai.yellow, .5 * monokai.black,  .5 * monokai.black,
            .5 * monokai.black,  .5 * monokai.black,  .5 * monokai.black,  .5 * monokai.black,
            .5 * monokai.brown,  .5 * monokai.gray,   .5 * monokai.purple, .5 * monokai.orange,
            .5 * monokai.black,  .5 * monokai.purple, .5 * monokai.orange, .5 * monokai.black,
            .5 * monokai.black,  .5 * monokai.yellow, .5 * monokai.blue,   .5 * monokai.black,
        };
        Soup3D outer_box = library.soups.box;
        outer_box.vertex_colors = vertex_colors;
        outer_box.primitive = SOUP_QUADS;
        outer_box.draw(PV * M4_Translation(0, 999.0, 0) * M4_Scaling(1000));
    }
}

// --------------------------------------------------

#define ORBIT_CAMERA 0
#define HUMAN_CAMERA 1
#define TRACK_CAMERA 2
#define PLANE_CAMERA 3
#define NO_ACTIVE_CAMERA -1
#define NUM_CAMERAS 4
#define INDEX2GRID(i) V2((i) % 2, (i) / 2) // 0 -> (0, 0); 1 -> (1, 0); ...

void mini_world() {

    OrbitCamera orbit = {};
    FPSCamera human = {};
    TrackingCamera track = {};
    ArbitraryCamera plane = {};
    int selected_camera = NO_ACTIVE_CAMERA;
    bool _initialized = false;
    bool draw_camera_cubes = true;
    window_set_clear_color(0.1, 0.1, 0.1, 1.0);

    while (cow_begin_frame()) {

        { // gui
            { // selected_camera
                int tmp = selected_camera;
                gui_slider("selected_camera", &selected_camera, -1, 3, 0, COW_KEY_TAB, true);

                // fornow
                if ((tmp != selected_camera)) {
                    window_pointer_unlock();
                }
            }
            gui_checkbox("draw_camera_cubes", &draw_camera_cubes, 'z');
        }

        { // init
            if (!_initialized || gui_button("reset", 'r')) {
                _initialized = true;
                orbit = { 200, RAD(15), RAD(-30), RAD(75) };
                human = { V3(0, 10, -20), RAD(180), 0, RAD(60), };
                track = { V3(-50, 50, 50), &plane.origin, RAD(45) };
                plane = { V3(0, 100, -500), M4_RotationAboutYAxis(RAD(180)), RAD(45) };
                selected_camera = NO_ACTIVE_CAMERA;
            }
        }

        { // update
            if (selected_camera == ORBIT_CAMERA) {
                orbit_camera_move(&orbit);
            } else if (selected_camera == PLANE_CAMERA) {
                arbitrary_camera_move(&plane);
            } else if (selected_camera == HUMAN_CAMERA) {
                fps_camera_move(&human);
            }
        }

        { // draw

            // matrices for all cameras
            mat4 C_array[] = {
                orbit_camera_get_C(&orbit),
                fps_camera_get_C(&human),
                tracking_camera_get_C(&track),
                arbitrary_camera_get_C(&plane), };
            mat4 P_array[] = {
                _window_get_P_perspective(orbit._angle_of_view),
                _window_get_P_perspective(human._angle_of_view),
                _window_get_P_perspective(track._angle_of_view),
                _window_get_P_perspective(plane._angle_of_view), };
            mat4 V_array[NUM_CAMERAS]; {
                for (int i = 0; i < NUM_CAMERAS; ++i) {
                    V_array[i] = inverse(C_array[i]);
                }
            }

            { // draw
                { // split screen bars
                    eso_begin(globals.Identity, SOUP_LINES, 6.0, false, true);
                    eso_color(0.0, 0.0, 0.0);
                    eso_vertex(-1.0, 0.0);
                    eso_vertex( 1.0, 0.0);
                    eso_vertex(0.0, -1.0);
                    eso_vertex(0.0,  1.0);
                    eso_end();
                }

                vec2 _window_radius = window_get_size() / 2;

                for (int camera_index = 0; camera_index < NUM_CAMERAS; ++camera_index) {
                    vec2 grid = INDEX2GRID(camera_index);
                    vec2 sgn = 2 * grid - V2(1.0);
                    text_draw(globals.Identity,
                            (char *) (
                                (camera_index == ORBIT_CAMERA) ? "orbit" :
                                (camera_index == HUMAN_CAMERA) ? "human" :
                                (camera_index == TRACK_CAMERA) ? "track" :
                                "plane"),
                            V2(-.96, -.9) + grid,
                            color_kelly(camera_index), 24, {}, true);

                    mat4 P = M4_Translation(.5 * sgn.x, .5 * sgn.y) * M4_Scaling(.5) * P_array[camera_index];
                    mat4 V = V_array[camera_index];
                    mat4 PV = P * V;

                    glEnable(GL_SCISSOR_TEST); // https://registry.khronos.org/OpenGL-Refpages/es2.0/xhtml/glScissor.xml
                    glScissor(int(grid[0] * _window_radius.x), int(grid[1] * _window_radius.y), int(_window_radius.x), int(_window_radius.y));
                    {
                        draw_world(P, V);

                        if (draw_camera_cubes) { // camera boxes and axes
                            for (int i = 0; i < NUM_CAMERAS; ++i) {
                                if (camera_index != i && i != 1) { // don't draw ourself if "we are the camera"
                                    // we're using C *as* M, because we're *drawing C itself*
                                    draw_box_with_fake_shadows(PV, C_array[i] * M4_Scaling(3), color_kelly(i));
                                    library.soups.axes.draw(PV * C_array[i] * M4_Scaling(10.0), color_kelly(i), 5.0);
                                    { // (potentially) extended -z axis
                                        eso_begin(PV, SOUP_LINES, 5.0);
                                        vec3 o = transformPoint(C_array[i], V3(0., 0., 0.));
                                        vec3 dir = normalized(transformVector(C_array[i], V3(0, 0, -1)));
                                        double L = (i == ORBIT_CAMERA) ? orbit.distance : (i == TRACK_CAMERA) ? norm(track.origin - *track.target) : 100;
                                        vec3 color = color_kelly(i);
                                        eso_color(color);
                                        eso_vertex(o);
                                        eso_color(color, .5);
                                        eso_vertex(o + L * dir);
                                        eso_end();
                                    }
                                } else if (camera_index != i && i == 1) {
                                    library.meshes.teapot.draw(P, V * C_array[i] * M4_Scaling(10.0), globals.Identity, color_kelly(i));
                                }
                            }
                        }
                    }
                    glDisable(GL_SCISSOR_TEST);
                }
            }
        }

        { // gui
            { // picking
                if (globals._mouse_owner == COW_MOUSE_OWNER_NONE) {
                    int camera_hot = NO_ACTIVE_CAMERA;

                    auto gui_picker = [&](int camera_index) {
                        vec2 sgn = 2 * INDEX2GRID(camera_index) - V2(1.0);
                        if ((sgn.x * globals.mouse_position_NDC.x > 0) && (sgn.y * globals.mouse_position_NDC.y > 0)) {
                            camera_hot = camera_index;
                        }
                        if (camera_index == camera_hot) {
                            if (camera_index != selected_camera) {
                                eso_begin(globals.Identity, SOUP_QUADS);
                                eso_color(1.0, 1.0, 1.0, 0.3);
                                eso_vertex(0.0, 0.0);
                                eso_vertex(0.0, sgn.y);
                                eso_vertex(sgn.x, sgn.y);
                                eso_vertex(sgn.x, 0.0);
                                eso_end();
                            }
                        }
                        if (camera_index == selected_camera) {
                            eso_begin(globals.Identity, SOUP_LINE_LOOP, 24.0, false, true);
                            eso_color(color_kelly(camera_index));
                            eso_vertex(0.0, 0.0);
                            eso_vertex(0.0, sgn.y);
                            eso_vertex(sgn.x, sgn.y);
                            eso_vertex(sgn.x, 0.0);
                            eso_end();
                        }
                    };

                    gui_picker(HUMAN_CAMERA);
                    if ((selected_camera != HUMAN_CAMERA) || (!window_is_pointer_locked())) {
                        gui_picker(ORBIT_CAMERA);
                        gui_picker(TRACK_CAMERA);
                        gui_picker(PLANE_CAMERA);
                    }

                    if ((camera_hot != NO_ACTIVE_CAMERA) && globals.mouse_left_pressed) {
                        selected_camera = camera_hot;
                        if (selected_camera == HUMAN_CAMERA) {
                            window_pointer_lock();
                        }
                    }

                    if (globals.key_pressed[COW_KEY_ESCAPE]) {
                        if (selected_camera == HUMAN_CAMERA) { window_pointer_unlock(); }
                        selected_camera = NO_ACTIVE_CAMERA;
                    }
                }
            }

            { // tweaks
                if (selected_camera == ORBIT_CAMERA) {
                    gui_readout("distance", &orbit.distance);
                    gui_readout("theta", &orbit.theta);
                    gui_readout("phi", &orbit.phi);
                } else if (selected_camera == HUMAN_CAMERA) {
                    gui_readout("origin", &human.origin);
                    gui_readout("theta", &human.theta);
                    gui_readout("phi", &human.phi);
                    gui_printf("---");
                    gui_printf("click to lock pointer");
                    gui_printf("press Escape to unlock pointer");
                } else if (selected_camera == TRACK_CAMERA) {
                    if (gui_button("switch target", COW_KEY_SPACE)) {
                        if (track.target == &plane.origin) {
                            track.target = &human.origin;
                        } else {
                            track.target = &plane.origin;
                        }
                    }
                } else if (selected_camera == PLANE_CAMERA) {
                    gui_readout("altitude", &plane.origin.y);
                }
            }
        }

    }

}

int main() {
    APPS {
        APP(mini_world);
    }
    return 0;
}

