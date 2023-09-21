#include "PlayMode.hpp"

#include "LitColorTextureProgram.hpp"

#include "DrawLines.hpp"
#include "Mesh.hpp"
#include "Load.hpp"
#include "gl_errors.hpp"
#include "data_path.hpp"

#include <glm/gtc/type_ptr.hpp>

#include <random>

float angle_window = (float) (M_PI / 3);
bool roll_mode = false;
glm::vec3 camera_angle = glm::vec3(0.0f, 0.0f, 0.0f);
glm::vec3 player_pos = glm::vec3(0.0f, 0.0f, 0.0f);
glm::vec3 player_velocity = glm::vec3(0.0f, 0.0f, 0.0f);

glm::vec3 ping_angles[5];
int pings = 0;
int previewing_ping = -1;
float ping_time = 0.0f;

float flight_time = 0.0f;

Load<Scene> empty_scene(LoadTagDefault, []() -> Scene const *
{
    return new Scene(data_path("data/emptyscene.scene"),
                     [&](Scene &scene, Scene::Transform *transform, std::string const &mesh_name)
                     {

                     });
});

Load<Sound::Sample> ambience1(LoadTagDefault, []() -> Sound::Sample const *
{
    return new Sound::Sample(data_path("data/ambience1.wav"));
});

Load<Sound::Sample> ambience2(LoadTagDefault, []() -> Sound::Sample const *
{
    return new Sound::Sample(data_path("data/ambience2.wav"));
});

Load<Sound::Sample> ambience3(LoadTagDefault, []() -> Sound::Sample const *
{
    return new Sound::Sample(data_path("data/ambience3.wav"));
});

Load<Sound::Sample> ambience4(LoadTagDefault, []() -> Sound::Sample const *
{
    return new Sound::Sample(data_path("data/ambience4.wav"));
});

Load<Sound::Sample> ambience5(LoadTagDefault, []() -> Sound::Sample const *
{
    return new Sound::Sample(data_path("data/ambience5.wav"));
});

Load<Sound::Sample> engine_sound(LoadTagDefault, []() -> Sound::Sample const *
{
    return new Sound::Sample(data_path("data/engine.wav"));
});

Load<Sound::Sample> ping_sound(LoadTagDefault, []() -> Sound::Sample const *
{
    return new Sound::Sample(data_path("data/ping.wav"));
});

Load<Sound::Sample> crash_sound(LoadTagDefault, []() -> Sound::Sample const *
{
    return new Sound::Sample(data_path("data/crash.wav"));
});

float randomFloat()
{
    return (float) std::rand() / ((float) RAND_MAX);
}

struct Asteroid
{
    glm::vec3 pos;
    float size;
    float distance;
    glm::vec2 angle;
    float time_until_ping = 0;
    float ratio;

    Asteroid(float hAngle, float vAngle, float dist, float s, float r)
    {
        pos.y = dist * cos(vAngle) * cos(hAngle);
        pos.x = dist * sin(vAngle);
        pos.z = dist * sin(hAngle);
        angle.x = hAngle;
        angle.y = vAngle;
        distance = dist;
        size = s;
        ratio = r;
    }
};

std::vector<Asteroid*> asteroids;

GLuint asteroid_vao = 0;
Load< MeshBuffer > asteroid_mesh(LoadTagDefault, []() -> MeshBuffer const * {
    MeshBuffer const *ret = new MeshBuffer(data_path("data/asteroid.pnct"));
    asteroid_vao = ret->make_vao_for_program(lit_color_texture_program->program);
    return ret;
});

void add_asteroid_to_scene(Scene &scene, Asteroid *a)
{
    Mesh const &mesh = asteroid_mesh->lookup("asteroid");

    Scene::Transform *t = new Scene::Transform();
    t->position = a->pos;
    t->rotation = glm::normalize(
            glm::angleAxis(randomFloat() * (float)(M_PI * 2), glm::vec3(0.0f, 0.0f, 1.0f))
            * glm::angleAxis(randomFloat() * (float)(M_PI * 2), glm::vec3(1.0f, 0.0f, 0.0f))
            * glm::angleAxis(randomFloat() * (float)(M_PI * 2), glm::vec3(0.0f, 1.0f, 0.0f))
            * glm::angleAxis(float(M_PI) / 2.0f, glm::vec3(1.0f, 0.0f, 0.0f)));
    t->scale = glm::vec3(a->size, a->size, a->size);
    scene.drawables.emplace_back(t);
    Scene::Drawable &drawable = scene.drawables.back();

    drawable.pipeline = lit_color_texture_program_pipeline;

    drawable.pipeline.vao = asteroid_vao;
    drawable.pipeline.type = mesh.type;
    drawable.pipeline.start = mesh.start;
    drawable.pipeline.count = mesh.count;
}

GLuint crosshair_vao = 0;
Load< MeshBuffer > crosshair_mesh(LoadTagDefault, []() -> MeshBuffer const * {
    MeshBuffer const *ret = new MeshBuffer(data_path("data/crosshair.pnct"));
    crosshair_vao = ret->make_vao_for_program(lit_color_texture_program->program);
    return ret;
});

void add_crosshair_to_scene(Scene &scene)
{
    Mesh const &mesh = crosshair_mesh->lookup("Cone.002");

    Scene::Transform *t = new Scene::Transform();
    t->rotation = glm::normalize(
            glm::angleAxis((float)(M_PI) + camera_angle.x, glm::vec3(0.0f, 0.0f, 1.0f))
            * glm::angleAxis(-camera_angle.y, glm::vec3(1.0f, 0.0f, 0.0f))
            * glm::angleAxis(-camera_angle.z, glm::vec3(0.0f, 1.0f, 0.0f))
            * glm::angleAxis(float(M_PI) / 2.0f, glm::vec3(1.0f, 0.0f, 0.0f)));
    scene.drawables.emplace_back(t);
    Scene::Drawable &drawable = scene.drawables.back();

    drawable.pipeline = lit_color_texture_program_pipeline;

    drawable.pipeline.vao = crosshair_vao;
    drawable.pipeline.type = mesh.type;
    drawable.pipeline.start = mesh.start;
    drawable.pipeline.count = mesh.count;
}

Scene::Transform *crosshair_guide_transform = new Scene::Transform();
void load_level(int count, Scene &scene)
{
    scene.drawables.clear();
    asteroids.clear();
    pings = 0;
    player_pos = glm::vec3(0.0f, 0.0f, 0.0f);
    player_velocity = glm::vec3(0.0f, 0.0f, 0.0f);
    flight_time = 0.0f;
    roll_mode = false;

    for (int i = 0; i < count; i++)
    {
        float h = angle_window * (randomFloat() - 0.5f) * 1.4f;
        float v = angle_window * (randomFloat() - 0.5f) * 1.4f;
        float d = 100 + 500.0f * (randomFloat());
        float ratio = (randomFloat() + 0.5f);
        float s = d / 5.0f * ratio;
        Asteroid* a = new Asteroid(h, v, d, s, ratio);
        asteroids.emplace_back(a);
    }

    Mesh const &mesh = crosshair_mesh->lookup("Cone.002");

    Scene::Transform *t = crosshair_guide_transform;
    scene.drawables.emplace_back(t);
    Scene::Drawable &drawable = scene.drawables.back();

    drawable.pipeline = lit_color_texture_program_pipeline;

    drawable.pipeline.vao = crosshair_vao;
    drawable.pipeline.type = mesh.type;
    drawable.pipeline.start = mesh.start;
    drawable.pipeline.count = mesh.count;
}

void ping(Scene& scene, bool save)
{
    ping_time = 5.0f;
    Sound::play(*ping_sound, 1.0f, 0.0f);

    if (save)
    {
        ping_angles[pings].x = camera_angle.x;
        ping_angles[pings].y = camera_angle.y;
        ping_angles[pings].z = camera_angle.z;
        pings++;
    }

    for (unsigned int i = 0; i < asteroids.size(); i++)
    {
        asteroids[i]->time_until_ping = asteroids[i]->distance / 200.0f;
    }

    add_crosshair_to_scene(scene);
}

float time_to_ambience_change[5] = {0.0f, 0.0f, 0.0, 0.0f, 0.0f};
bool playing[5] = {false, false, false, false, false};
std::shared_ptr <Sound::PlayingSample> loops[5];
int current_difficulty = 3;

PlayMode::PlayMode() : scene(*empty_scene)
{
    load_level(current_difficulty, scene);

    //get pointer to camera for convenience:
    if (scene.cameras.size() != 1)
        throw std::runtime_error(
                "Expecting scene to have exactly one camera, but it has " + std::to_string(scene.cameras.size()));
    camera = &scene.cameras.front();

    //start music loop playing:
    // (note: position will be over-ridden in update())
    //leg_tip_loop = Sound::loop_3D(*dusty_floor_sample, 0.0f, get_leg_tip_position(), 10.0f);

    loops[0] = Sound::loop(*ambience1, 0.0f, 0.0f);
    loops[1] = Sound::loop(*ambience2, 0.0f, 0.0f);
    loops[2] = Sound::loop(*ambience3, 0.0f, 0.0f);
    loops[3] = Sound::loop(*ambience4, 0.0f, 0.0f);
    loops[4] = Sound::loop(*ambience5, 0.0f, 0.0f);

    for (int i = 0; i < 5; i++)
    {
        time_to_ambience_change[i] = randomFloat() * 15.0f + 1.0f;
    }
}

PlayMode::~PlayMode()
{

}


float clamp(float min, float num, float max)
{
    if (num < min)
        return min;
    else if (num > max)
        return max;
    else return num;
}

void set_preview_ping(int slot)
{
    if (ping_time <= 0 && !roll_mode && pings > slot && flight_time <= 0.0f)
    {
        previewing_ping = slot;
    }
}

std::shared_ptr< Sound::PlayingSample > current_engine_sound;

bool PlayMode::handle_event(SDL_Event const &evt, glm::uvec2 const &window_size)
{
    if (evt.type == SDL_KEYDOWN)
    {
        if (evt.key.keysym.sym == SDLK_ESCAPE)
        {
            SDL_SetRelativeMouseMode(SDL_FALSE);
            return true;
        }
        else if (evt.key.keysym.sym == SDLK_a)
        {
            left.downs += 1;
            left.pressed = true;
            return true;
        }
        else if (evt.key.keysym.sym == SDLK_d)
        {
            right.downs += 1;
            right.pressed = true;
            return true;
        }
        else if (evt.key.keysym.sym == SDLK_w)
        {
            up.downs += 1;
            up.pressed = true;
            return true;
        }
        else if (evt.key.keysym.sym == SDLK_s)
        {
            down.downs += 1;
            down.pressed = true;
            return true;
        }
        else if (evt.key.keysym.sym == SDLK_1)
        {
            set_preview_ping(0);
            return true;
        }
        else if (evt.key.keysym.sym == SDLK_2)
        {
            set_preview_ping(1);
            return true;
        }
        else if (evt.key.keysym.sym == SDLK_3)
        {
            set_preview_ping(2);
            return true;
        }
        else if (evt.key.keysym.sym == SDLK_4)
        {
            set_preview_ping(3);
            return true;
        }
        else if (evt.key.keysym.sym == SDLK_5)
        {
            set_preview_ping(4);
            return true;
        }
        else if (evt.key.keysym.sym == SDLK_0)
        {
            previewing_ping = -1;
            return true;
        }
        else if (evt.key.keysym.sym == SDLK_SPACE)
        {
            if (flight_time <= 0.0f)
            {
                if (previewing_ping >= 0)
                    ping(scene, false);
                else if (pings < 5)
                {
                    if (!roll_mode)
                        roll_mode = true;
                    else if (ping_time <= 0.0f)
                        ping(scene, true);
                }
            }
            return true;
        }
        else if (evt.key.keysym.sym == SDLK_RETURN)
        {
            if (flight_time <= 0.0f)
            {
                flight_time = 12.0f;
                current_engine_sound = Sound::play(*engine_sound, 0.5f, 0.0f);

                for (Asteroid *a: asteroids)
                {
                    add_asteroid_to_scene(scene, a);
                }

                return true;
            }
        }
    }
    else if (evt.type == SDL_KEYUP)
    {
        if (evt.key.keysym.sym == SDLK_a)
        {
            left.pressed = false;
            return true;
        }
        else if (evt.key.keysym.sym == SDLK_d)
        {
            right.pressed = false;
            return true;
        }
        else if (evt.key.keysym.sym == SDLK_w)
        {
            up.pressed = false;
            return true;
        }
        else if (evt.key.keysym.sym == SDLK_s)
        {
            down.pressed = false;
            return true;
        }
    }
    else if (evt.type == SDL_MOUSEBUTTONDOWN)
    {
        if (SDL_GetRelativeMouseMode() == SDL_FALSE)
        {
            SDL_SetRelativeMouseMode(SDL_TRUE);
            return true;
        }
    }
    else if (evt.type == SDL_MOUSEMOTION)
    {
        if (SDL_GetRelativeMouseMode() == SDL_TRUE)
        {
            if (ping_time <= 0.0f && previewing_ping < 0 && flight_time <= 0.0f)
            {
                glm::vec2 motion = glm::vec2(
                        -evt.motion.xrel / float(window_size.y),
                        -evt.motion.yrel / float(window_size.y)
                );

                if (!roll_mode)
                {
                    camera_angle.x += motion.x;
                    camera_angle.y += motion.y;
                    camera_angle.x = clamp(-angle_window / 2.0f, camera_angle.x, angle_window / 2.0f);
                    camera_angle.y = clamp(-angle_window / 2.0f, camera_angle.y, angle_window / 2.0f);
                }
                else
                    camera_angle.z += motion.x + motion.y;
            }
            return true;
        }
    }

    return false;
}

void PlayMode::update(float elapsed)
{
    camera->fovy = 70;

    for (int i = 0; i < 5; i++)
    {
        time_to_ambience_change[i] -= elapsed;

        if (time_to_ambience_change[i] <= 0)
        {
            time_to_ambience_change[i] = randomFloat() * 15.0f + 4.0f;
            playing[i] = !playing[i];

            if (playing[i])
            {
                float vol = (randomFloat() * (0.75f / (i + 1)) + 0.25f) / 2.0f;
                float pan = randomFloat() * 2.0f - 1.0f;

                loops[i]->set_volume(vol, 4.0f);
                loops[i]->set_pan(pan, 4.0f);
            }
            else
            {
                if (time_to_ambience_change[i] >= 8.0f)
                    time_to_ambience_change[i] /= 2;
                loops[i]->set_volume(0.0f, 4.0f);
                loops[i]->set_pan(0.0f, 4.0f);
            }
        }
    }

    if (previewing_ping >= 0)
    {
        float speed = elapsed * 5.0f;

        if (camera_angle.x < ping_angles[previewing_ping].x)
            camera_angle.x = clamp(camera_angle.x, camera_angle.x + speed, ping_angles[previewing_ping].x);

        if (camera_angle.x > ping_angles[previewing_ping].x)
            camera_angle.x = clamp(ping_angles[previewing_ping].x, camera_angle.x - speed, camera_angle.x);

        if (camera_angle.y < ping_angles[previewing_ping].y)
            camera_angle.y = clamp(camera_angle.y, camera_angle.y + speed, ping_angles[previewing_ping].y);

        if (camera_angle.y > ping_angles[previewing_ping].y)
            camera_angle.y = clamp(ping_angles[previewing_ping].y, camera_angle.y - speed, camera_angle.y);

        if (camera_angle.z < ping_angles[previewing_ping].z)
            camera_angle.z = clamp(camera_angle.z, camera_angle.z + speed, ping_angles[previewing_ping].z);

        if (camera_angle.z > ping_angles[previewing_ping].z)
            camera_angle.z = clamp(ping_angles[previewing_ping].z, camera_angle.z - speed, camera_angle.z);
    }

    if (ping_time > 0.0f)
    {
        ping_time -= elapsed;

        for (unsigned int i = 0; i < asteroids.size(); i++)
        {
            if (asteroids[i]->time_until_ping > 0.0f)
            {
                asteroids[i]->time_until_ping -= elapsed;
                if (asteroids[i]->time_until_ping <= 0.0f)
                {
                    float dist = 10.0f * sqrt(pow(asteroids[i]->angle.y + camera_angle.x, 2.0f) + pow(asteroids[i]->angle.x - camera_angle.y, 2.0f));
                    dist /= asteroids[i]->ratio;
                    //printf("dist = %f | %f %f | %f %f\n", dist, camera_angle.y, camera_angle.x, asteroids[i]->angle.x, asteroids[i]->angle.y);
                    if (dist < 1.0f)
                        dist = 1.0f;

                    Sound::play_3D(*ping_sound, 1.0f / dist, asteroids[i]->pos);
                }
            }
        }

        if (ping_time <= 0.0f)
            roll_mode = false;
    }

    if (flight_time > 0.0f)
    {
        float launchAccel = 10.0f * elapsed;
        player_velocity.x += -sin(camera_angle.x) * cos(camera_angle.y) * launchAccel;
        player_velocity.y += cos(camera_angle.x) * cos(camera_angle.y) * launchAccel;
        player_velocity.z += sin(camera_angle.y) * launchAccel;

        player_pos += player_velocity * elapsed;
        flight_time -= elapsed;

        bool crashed = false;
        float player_size = 50;
        for (Asteroid *a: asteroids)
        {
            float dist = (float) (pow(a->pos.x - player_pos.x, 2) + pow(a->pos.y - player_pos.y, 2) + pow(a->pos.z - player_pos.z, 2));
            //printf("dist: %f\n", dist);
            if (dist <= pow((player_size + a->size) / 2.0f, 2.0f))
            {
                crashed = true;
                current_engine_sound->stop();
                Sound::play(*crash_sound, 1.0f, 0.0f);
                current_difficulty -= 1;
                load_level(current_difficulty, scene);
            }
        }

        if (flight_time <= 0.0f && !crashed)
        {
            current_difficulty += 1;
            load_level(current_difficulty, scene);
        }
    }

    //move camera:
    {
        while (camera_angle.z > (float)(M_PI))
            camera_angle.z -= (float)(M_PI * 2);

        while (camera_angle.z < -(float)(M_PI))
            camera_angle.z += (float)(M_PI * 2);

        if (!roll_mode && previewing_ping < 0)
        {
            if (camera_angle.z > 0)
                camera_angle.z = clamp(0.0f, camera_angle.z - 5.0f * elapsed, (float)(M_PI));

            if (camera_angle.z < 0)
                camera_angle.z = clamp(-(float)(M_PI), camera_angle.z + 5.0f * elapsed, 0.0f);
        }

        camera->transform->position = player_pos;
        camera->transform->rotation = glm::normalize(
                glm::angleAxis(camera_angle.x, glm::vec3(0.0f, 0.0f, 1.0f))
                * glm::angleAxis(camera_angle.y, glm::vec3(1.0f, 0.0f, 0.0f))
                  * glm::angleAxis(camera_angle.z, glm::vec3(0.0f, 1.0f, 0.0f))
                  * glm::angleAxis(float(M_PI) / 2.0f, glm::vec3(1.0f, 0.0f, 0.0f))
        );

        crosshair_guide_transform->rotation = glm::normalize(
                glm::angleAxis((float)(M_PI) + camera_angle.x, glm::vec3(0.0f, 0.0f, 1.0f))
                * glm::angleAxis(-camera_angle.y, glm::vec3(1.0f, 0.0f, 0.0f))
                * glm::angleAxis(-camera_angle.z, glm::vec3(0.0f, 1.0f, 0.0f))
                * glm::angleAxis(float(M_PI) / 2.0f, glm::vec3(1.0f, 0.0f, 0.0f)));

        if (!roll_mode)
        {
            crosshair_guide_transform->scale.x = 0.0f;
            crosshair_guide_transform->scale.y = 0.0f;
            crosshair_guide_transform->scale.z = 0.0f;
        }
        else
        {
            crosshair_guide_transform->scale.x = 1.0f;
            crosshair_guide_transform->scale.y = 1.0f;
            crosshair_guide_transform->scale.z = 1.0f;
        }
    }

    { //update listener to camera position:
        glm::mat4x3 frame = camera->transform->make_local_to_parent();
        glm::vec3 frame_right = frame[0];
        glm::vec3 frame_at = frame[3];
        Sound::listener.set_position_right(frame_at, frame_right, 1.0f / 60.0f);
    }

    //reset button press counters:
    left.downs = 0;
    right.downs = 0;
    up.downs = 0;
    down.downs = 0;
}

void PlayMode::draw(glm::uvec2 const &drawable_size)
{
    //update camera aspect ratio for drawable:
    camera->aspect = float(drawable_size.x) / float(drawable_size.y);

    //set up light type and position for lit_color_texture_program:
    // TODO: consider using the Light(s) in the scene to do this
    glUseProgram(lit_color_texture_program->program);
    glUniform1i(lit_color_texture_program->LIGHT_TYPE_int, 1);
    glUniform3fv(lit_color_texture_program->LIGHT_DIRECTION_vec3, 1, glm::value_ptr(glm::vec3(0.0f, 0.0f, -1.0f)));
    glUniform3fv(lit_color_texture_program->LIGHT_ENERGY_vec3, 1, glm::value_ptr(glm::vec3(1.0f, 1.0f, 0.95f)));
    glUseProgram(0);

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClearDepth(1.0f); //1.0 is actually the default value to clear the depth buffer to, but FYI you can change it.
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS); //this is the default depth comparison function, but FYI you can change it.

    scene.draw(*camera);

    { //use DrawLines to overlay some text:
        glDisable(GL_DEPTH_TEST);
        float aspect = float(drawable_size.x) / float(drawable_size.y);
        DrawLines lines(glm::mat4(
                1.0f / aspect, 0.0f, 0.0f, 0.0f,
                0.0f, 1.0f, 0.0f, 0.0f,
                0.0f, 0.0f, 1.0f, 0.0f,
                0.0f, 0.0f, 0.0f, 1.0f
        ));

        std::string text;
        if (flight_time > 0.0f)
            text = "Blastoff! Hopefully you picked a good trajectory!";
        else if (ping_time > 0.0f)
            text = "Listen carefully for echoes from asteroids";
        else if (roll_mode)
            text = "Move the mouse to roll the camera; Press Space to ping";
        else if (previewing_ping >= 0)
            text = "Press Space to replay the ping; 0 to exit preview mode";
        else
        {
            if (pings >= 5)
                text = "You're at the ping limit; Press Num Keys to see past pings or Enter to blastoff!";
            else if (pings > 0)
                text = "Move the mouse to set angle for ping; Space to prepare ping; Num Keys to see past pings";
            else
                text = "Move the mouse to set angle for ping; Space to prepare ping";
        }

        constexpr float H = 0.09f;
        lines.draw_text(text,
                        glm::vec3(-aspect + 0.1f * H, -1.0 + 0.1f * H, 0.0),
                        glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
                        glm::u8vec4(0x00, 0x00, 0x00, 0x00));
        float ofs = 2.0f / drawable_size.y;
        lines.draw_text(text,
                        glm::vec3(-aspect + 0.1f * H + ofs, -1.0 + +0.1f * H + ofs, 0.0),
                        glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
                        glm::u8vec4(0xff, 0xff, 0xff, 0x00));
    }
    GL_ERRORS();
}