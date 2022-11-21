#include <SFML/Graphics.hpp>
#include <iostream>
#include <fstream>
#include <math.h>
#include <thread>
#include <vector>

using namespace sf;
using namespace std;

const float PI = 3.14159265358979323846f;
int WIDTH = 1200;
int HEIGHT = 720;

#include "../../Libraries/Library.h"
Camera3d defaultCam = Camera3d(Vector3ff(0, 50, -200));
// vector<Particle> particles = vector<Particle>();

float maxFPS = 60;
float deltaTime = 0;
Clock frameStartTime;
Text FPSDisplay;
Font defaultFont;
Clock fpsDisplayUpdate;

Vector2i oldMousePosition;
bool isMouseClicked;

bool isKeyboardClicked;
Keyboard::Key keyClicked;

Shader withShadowDrawing;

float DeltaT = 1;

class NeuroWeb
{
public:
    vector<vector<float>> nodes;
    vector<vector<vector<float>>> weights;
    NeuroWeb(vector<vector<float>> nodes)
    {
        this->nodes = nodes;

        for (int i = 1; i < this->nodes.size(); i++)
        {
            for (int j = 0; j < this->nodes[i].size(); j++)
            {
                this->weights[i][j].push_back(-1 + random() * 2);
            }
        }
    }
    vector<float> GetLastNodes()
    {
        for (int i = 1; i < this->nodes.size(); i++)
        {
            for (int j = 0; j < this->nodes[i].size(); j++)
            {

                this->nodes[i][j] = 0;
                for (int k = 0; k < this->weights[i][j].size(); k++)
                {
                    this->nodes[i][j] += this->nodes[i - 1][k] * this->weights[i][j][k];
                }
            }
        }
        return this->nodes[this->nodes.size() - 1];
    }
};

class Celestial
{
private:
    CircleShape body;

public:
    Vector3ff position, velocity;
    float mass, radius;
    Color color;
    Celestial(Vector3ff StartPosition, Vector3ff StartVelocity, float mass, float radius, Color color)
    {
        this->position = StartPosition;
        this->velocity = StartVelocity;
        this->mass = mass;
        this->radius = radius;
        this->color = color;

        this->body = CircleShape(1.0f);
        this->body.setFillColor(this->color);
    }
    void update()
    {
        this->position += this->velocity * DeltaT;
    }
    void draw(RenderWindow *window)
    {
        Vector2f screenPosition = defaultCam.ProjectToCanvas(this->position);
        if (!defaultCam.isOnScreen(screenPosition))
            return;
        this->body.setPosition(screenPosition);

        Vector3ff p2 = this->position + defaultCam.up.cross(this->position - defaultCam.position).normalize() * this->radius;

        float r = magnitude(screenPosition - defaultCam.ProjectToCanvas(p2));

        if (r < 0.4)
            r = 0.4;
        this->body.setRadius(r);
        this->body.setOrigin(r, r);

        withShadowDrawing.setUniform("targetPosition", Glsl::Vec3(this->position.x, this->position.y, this->position.z));
        withShadowDrawing.setUniform("targetScreenPosition", Glsl::Vec2{screenPosition});
        withShadowDrawing.setUniform("targetRadius", this->radius);
        withShadowDrawing.setUniform("targetScreenRadius", r);
        withShadowDrawing.setUniform("targetColor", Glsl::Vec4{this->color});

        window->draw(this->body);
    }
    bool attractTo(Celestial *target)
    {
        Vector3ff delta = target->position - this->position;
        float magn = delta.magnitude();
        if (magn < this->radius + target->radius)
            return false;

        this->velocity += (delta / magn) * (target->mass / powf(magn, 2) - target->mass / powf(magn, 3)) * DeltaT;
        // this->velocity += (delta / magn) * (target->mass / powf(magn, 2)) * DeltaT;
        return true;
    }
    void onCollide(Celestial *target)
    {
        Vector3ff averageVelocity = (this->velocity * this->mass + target->velocity * target->mass) / (this->mass + target->mass);
        this->velocity = averageVelocity;
        target->velocity = averageVelocity;
    }
    vector<int> getCollisions(vector<Celestial> *cels)
    {
        vector<int> indexes;
        for (int i = 0; i < cels->size(); i++)
        {
            Celestial body = (*cels)[i];
            float magn = (body.position - this->position).magnitude();
            if (magn < this->radius + body.radius)
            {
                indexes.push_back(i);
            }
        }
        return indexes;
    }
};

vector<Celestial> celestials;

float lightness = 1.0f;
bool isPaused = false;

bool threadsComplete[4] = {true, true, true, true};
void gravityUpdate(int id = 0)
{
    threadsComplete[id] = false;
    for (int i = celestials.size() / 4 * id; i < celestials.size() / 4 * (id + 1); i++)
    {
        for (int j = 0; j < celestials.size(); j++)
        {
            if (i == j)
                continue;
            if (!celestials[i].attractTo(&celestials[j]))
            {
                celestials[i].onCollide(&celestials[j]);
            }
        }
    }
    threadsComplete[id] = true;
}
Thread gravityThread1(&gravityUpdate, 0);
Thread gravityThread2(&gravityUpdate, 1);
Thread gravityThread3(&gravityUpdate, 2);

void sortCelestials()
{
    bool direction = false;
    while (true)
    {
        int swapsCount = 0;
        if (direction)
        {
            for (int i = 1; i < celestials.size(); i++)
            {
                float l1 = (celestials[i].position - defaultCam.position).magnitude();
                float l2 = ((celestials[i - 1].position - defaultCam.position).magnitude());

                if (l2 < l1)
                {
                    swap(celestials[i], celestials[i - 1]);
                    swapsCount++;
                }
            }
        }
        else
        {
            for (int i = celestials.size() - 1 - 1; i >= 0; i--)
            {
                float l1 = (celestials[i].position - defaultCam.position).magnitude();
                float l2 = ((celestials[i + 1].position - defaultCam.position).magnitude());

                if (l2 > l1)
                {
                    swap(celestials[i], celestials[i + 1]);
                    swapsCount++;
                }
            }
        }
        if (swapsCount == 0)
            break;
        direction = !direction;
    }
}

int main()
{
    cout << "Start";
    ContextSettings settings;
    settings.antialiasingLevel = 1;
    RenderWindow window(VideoMode(WIDTH, HEIGHT), "SFML window", Style::Default, settings);
    window.setFramerateLimit(60);

    defaultFont = Font();
    defaultFont.loadFromFile("E:\\!Coding\\c++\\Projects\\SFML\\Default\\sansation.ttf");

    FPSDisplay.setFillColor(Color::Green);
    FPSDisplay.setFont(defaultFont);
    FPSDisplay.setCharacterSize(16);
    fpsDisplayUpdate = Clock();

    frameStartTime = Clock();

    if (!withShadowDrawing.loadFromFile("E:\\!Coding\\c++\\Projects\\SFML\\3d Celestials\\src\\shadowDrawing.frag", Shader::Fragment))
    {
        cerr << "Nope" << endl;
        cin;
    }

    celestials.push_back(Celestial(Vector3ff(), Vector3ff(), 1, 5, Color::Yellow));
    // celestials.push_back(Celestial(Vector3ff(10), Vector3ff(), 1, 5, Color::White));

    for (int i = 0; i < 900; i++)
    {
        float a = random() * PI * 2;
        float dst = 1000 + random() * 100;

        float v = -sqrtf(celestials[0].mass / dst);
        celestials.push_back(Celestial(Vector3ff(cosf(a), (-1 + random() * 2) * 0.05, sinf(a)) * dst, Vector3ff(cosf(a + PI / 2), 0, sinf(a + PI / 2)) * v, 0.001, 0.4, Color(80, 80, 80, 255)));
    }
    cout << RAND_MAX << endl;

    while (window.isOpen())
    {
        if (!threadsComplete[0] || !threadsComplete[1] || !threadsComplete[2] || !threadsComplete[3])
            continue;
        WIDTH = window.getSize().x;
        HEIGHT = window.getSize().y;
        deltaTime = frameStartTime.getElapsedTime().asSeconds();
        // if (deltaTime < 1 / maxFPS)
        //     continue;
        frameStartTime.restart();

        if (fpsDisplayUpdate.getElapsedTime().asSeconds() > 0.12f)
        {
            FPSDisplay.setString(to_string((int)round(1 / deltaTime)));
            fpsDisplayUpdate.restart();
        }

        Event event;
        while (window.pollEvent(event))
        {
            if (event.type == Event::Closed)
                window.close();
            if (event.type == Event::MouseButtonPressed)
            {

                isMouseClicked = true;
            }
            if (event.type == Event::MouseButtonReleased)
            {
                isMouseClicked = false;
            }
            if (event.type == Event::MouseMoved)
            {
                Vector2i mousePosition = Mouse::getPosition();
                if (isMouseClicked)
                {
                    defaultCam.Rotate(Vector3ff(defaultCam.rotation.x + (mousePosition.x - oldMousePosition.x) * 0.001f, defaultCam.rotation.y - (mousePosition.y - oldMousePosition.y) * 0.001f, defaultCam.rotation.z));
                }
                oldMousePosition = mousePosition;
            }
            if (event.type == Event::KeyPressed)
            {
                keyClicked = event.key.code;
                if (keyClicked == Keyboard::Space)
                    isPaused = !isPaused;
                isKeyboardClicked = true;
            }
            if (event.type == Event::KeyReleased)
            {
                isKeyboardClicked = false;
            }
            if (event.type == Event::MouseWheelMoved)
            {
                if (event.mouseWheel.delta == 1)
                {
                    DeltaT += 0.2;
                }
                else
                {
                    DeltaT += -0.2;
                }
                cout << DeltaT << endl;
            }
        }
        if (isKeyboardClicked)
        {
            float speed = 100 * deltaTime;
            if (keyClicked == Keyboard::A)
            {
                defaultCam.position += defaultCam.right * -1.0f * speed;
            }
            if (keyClicked == Keyboard::W)
            {
                defaultCam.position += defaultCam.forward * speed;
            }
            if (keyClicked == Keyboard::S)
            {
                defaultCam.position += defaultCam.forward * -1.0f * speed;
            }
            if (keyClicked == Keyboard::D)
            {
                defaultCam.position += defaultCam.right * speed;
            }
            if (keyClicked == Keyboard::R)
            {
                defaultCam.position += defaultCam.up * speed;
            }
            if (keyClicked == Keyboard::F)
            {
                defaultCam.position += defaultCam.up * -1.0f * speed;
            }
        }

        withShadowDrawing.setUniform("u_resolution", Glsl::Vec2{window.getSize()});
        withShadowDrawing.setUniform("cameraPosition", Glsl::Vec3(defaultCam.position.x, defaultCam.position.y, defaultCam.position.z));
        withShadowDrawing.setUniform("lightSourcePosition", Glsl::Vec3(celestials[0].position.x, celestials[0].position.y, celestials[0].position.z));
        withShadowDrawing.setUniform("cameraRight", Glsl::Vec3(defaultCam.right.x, defaultCam.right.y, defaultCam.right.z));
        withShadowDrawing.setUniform("cameraUp", Glsl::Vec3(defaultCam.up.x, defaultCam.up.y, defaultCam.up.z));
        withShadowDrawing.setUniform("cameraForward", Glsl::Vec3(defaultCam.forward.x, defaultCam.forward.y, defaultCam.forward.z));
        withShadowDrawing.setUniform("lightness", lightness);

        // sortCelestials();
        //  Clock gravityTest = Clock();
        for (float t = 0; t < 1; t += 1)
        {
            gravityThread1.launch();
            gravityThread2.launch();
            gravityThread3.launch();
            gravityUpdate(3);
            // cout << gravityTest.getElapsedTime().asMilliseconds() << endl;
            // gravityTest.restart();

            for (int i = 0; i < celestials.size(); i++)
            {
                celestials[i].update();
            }
        }

        for (int i = 0; i < celestials.size(); i++)
        {

            celestials[i].draw(&window);
        }
        // cout << gravityTest.getElapsedTime().asMilliseconds() << endl;
        // cout << "------" << endl;

        window.draw(FPSDisplay);
        window.display();
        window.clear();
    }
    return 0;
}