#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <learnopengl/filesystem.h>
#include <learnopengl/shader.h>
#include <learnopengl/camera.h>
#include <learnopengl/model.h>
#include <math.h>

#include <iostream>

void framebuffer_size_callback(GLFWwindow *window, int width, int height);

void mouse_callback(GLFWwindow *window, double xpos, double ypos);

void scroll_callback(GLFWwindow *window, double xoffset, double yoffset);

void processInput(GLFWwindow *window);

void key_callback(GLFWwindow *window, int key, int scancode, int action, int mods);

unsigned int loadCubemap(std::vector<std::string>& faces);

unsigned int loadTexture(char const * path);

// settings
const unsigned int SCR_WIDTH = 1280;
const unsigned int SCR_HEIGHT = 960;

// camera

float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true;

// timing
float deltaTime = 0.0f;
float lastFrame = 0.0f;

bool bBloom = true;

struct PointLight {
    glm::vec3 position;
    glm::vec3 ambient;
    glm::vec3 diffuse;
    glm::vec3 specular;

    float constant;
    float linear;
    float quadratic;
};

struct DirLight {
    glm::vec3 direction;
    glm::vec3 ambient;
    glm::vec3 diffuse;
    glm::vec3 specular;
};

struct SpotLight{
    glm::vec3 position;
    glm::vec3 direction;
    float cutOff;
    float outerCutOff;

    float constant;
    float linear;
    float quadratic;

    glm::vec3 ambient;
    glm::vec3 diffuse;
    glm::vec3 specular;
};

struct ProgramState {
    glm::vec3 clearColor = glm::vec3(0);
    bool ImGuiEnabled = false;
    Camera camera;
    bool CameraMouseMovementUpdateEnabled = true;
    glm::vec3 backpackPosition = glm::vec3(0.0f);
    float backpackScale = 1.0f;
    PointLight pointLight;
    ProgramState()
            : camera(glm::vec3(0.0f, 0.0f, 3.0f)) {}

    void SaveToFile(std::string filename);

    void LoadFromFile(std::string filename);
};

void ProgramState::SaveToFile(std::string filename) {
    std::ofstream out(filename);
    out << clearColor.r << '\n'
        << clearColor.g << '\n'
        << clearColor.b << '\n'
        << ImGuiEnabled << '\n'
        << camera.Position.x << '\n'
        << camera.Position.y << '\n'
        << camera.Position.z << '\n'
        << camera.Front.x << '\n'
        << camera.Front.y << '\n'
        << camera.Front.z << '\n';
}

void ProgramState::LoadFromFile(std::string filename) {
    std::ifstream in(filename);
    if (in) {
        in >> clearColor.r
           >> clearColor.g
           >> clearColor.b
           >> ImGuiEnabled
           >> camera.Position.x
           >> camera.Position.y
           >> camera.Position.z
           >> camera.Front.x
           >> camera.Front.y
           >> camera.Front.z;
    }
}

ProgramState *programState;

void DrawImGui(ProgramState *programState);

int main() {
    // glfw: initialize and configure
    // ------------------------------
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    // msaa
    glfwWindowHint(GLFW_SAMPLES, 4);

    // glfw window creation
    // --------------------
    GLFWwindow *window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Farming_Life", NULL, NULL);
    if (window == NULL) {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetKeyCallback(window, key_callback);
    // tell GLFW to capture our mouse
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    // glad: load all OpenGL function pointers
    // ---------------------------------------
    if (!gladLoadGLLoader((GLADloadproc) glfwGetProcAddress)) {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    // tell stb_image.h to flip loaded texture's on the y-axis (before loading model).
    stbi_set_flip_vertically_on_load(true);

    programState = new ProgramState;
    programState->LoadFromFile("resources/program_state.txt");
    if (programState->ImGuiEnabled) {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }
    // Init Imgui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    (void) io;



    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330 core");

    // configure global opengl state
    // -----------------------------
    // build and compile shaders
    // -------------------------
    Shader ourShader("resources/shaders/model_lighting.vs", "resources/shaders/model_lighting.fs");
    Shader skyboxShader("resources/shaders/skybox.vs", "resources/shaders/skybox.fs");
    Shader blendingShader("resources/shaders/blendShader.vs", "resources/shaders/blendShader.fs");
    Shader hdrShader("resources/shaders/hdr.vs", "resources/shaders/hdr.fs");
    Shader blurShader("resources/shaders/blur.vs", "resources/shaders/blur.fs");
    Shader bloomShader("resources/shaders/bloom.vs", "resources/shaders/bloom.fs");

    // skybox vertices
    stbi_set_flip_vertically_on_load(false);

    float skyboxVertices[] = {
            // positions
            -1.0f,  1.0f, -1.0f,
            -1.0f, -1.0f, -1.0f,
             1.0f, -1.0f, -1.0f,
             1.0f, -1.0f, -1.0f,
             1.0f,  1.0f, -1.0f,
            -1.0f,  1.0f, -1.0f,

            -1.0f, -1.0f,  1.0f,
            -1.0f, -1.0f, -1.0f,
            -1.0f,  1.0f, -1.0f,
            -1.0f,  1.0f, -1.0f,
            -1.0f,  1.0f,  1.0f,
            -1.0f, -1.0f,  1.0f,

            1.0f, -1.0f, -1.0f,
            1.0f, -1.0f,  1.0f,
            1.0f,  1.0f,  1.0f,
            1.0f,  1.0f,  1.0f,
            1.0f,  1.0f, -1.0f,
            1.0f, -1.0f, -1.0f,

            -1.0f, -1.0f,  1.0f,
            -1.0f,  1.0f,  1.0f,
            1.0f,  1.0f,  1.0f,
            1.0f,  1.0f,  1.0f,
            1.0f, -1.0f,  1.0f,
            -1.0f, -1.0f,  1.0f,

            -1.0f,  1.0f, -1.0f,
            1.0f,  1.0f, -1.0f,
            1.0f,  1.0f,  1.0f,
            1.0f,  1.0f,  1.0f,
            -1.0f,  1.0f,  1.0f,
            -1.0f,  1.0f, -1.0f,

            -1.0f, -1.0f, -1.0f,
            -1.0f, -1.0f,  1.0f,
            1.0f, -1.0f, -1.0f,
            1.0f, -1.0f, -1.0f,
            -1.0f, -1.0f,  1.0f,
            1.0f, -1.0f,  1.0f,
    };

    float quadVertices[] = {
            // positions        // texture Coords
            -1.0f,  1.0f, 0.0f, 0.0f, 1.0f,
            -1.0f, -1.0f, 0.0f, 0.0f, 0.0f,
            1.0f,  1.0f, 0.0f, 1.0f, 1.0f,
            1.0f, -1.0f, 0.0f, 1.0f, 0.0f,
    };



    float grassVertices[] = {
            // positions         // texture Coords
            0.0f,  0.5f,  0.0f,  0.0f,  0.0f,
            0.0f, -0.5f,  0.0f,  0.0f,  1.0f,
            1.0f, -0.5f,  0.0f,  1.0f,  1.0f,

            0.0f,  0.5f,  0.0f,  0.0f,  0.0f,
            1.0f, -0.5f,  0.0f,  1.0f,  1.0f,
            1.0f,  0.5f,  0.0f,  1.0f,  0.0f
    };

    glEnable(GL_MULTISAMPLE);

    unsigned int hdrFBO;
    glGenFramebuffers(1, &hdrFBO);

    glBindFramebuffer(GL_FRAMEBUFFER, hdrFBO);

    unsigned int colorBuffers[2];
    glGenTextures(2, colorBuffers);
    for (unsigned int i = 0; i < 2; i++)
    {
        glBindTexture(GL_TEXTURE_2D, colorBuffers[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, SCR_WIDTH, SCR_HEIGHT, 0, GL_RGBA, GL_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);  // we clamp to the edge as the blur filter would otherwise sample repeated texture values!
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        // attach texture to framebuffer
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, GL_TEXTURE_2D, colorBuffers[i], 0);
    }

    unsigned int rboDepth;
    glGenRenderbuffers(1, &rboDepth);
    glBindRenderbuffer(GL_RENDERBUFFER, rboDepth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, SCR_WIDTH, SCR_HEIGHT);

    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rboDepth);
    // attach buffers

    unsigned int attachments[2] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
    glDrawBuffers(2, attachments);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cout << "Framebuffer not complete!" << std::endl;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    unsigned int pingpongFBO[2];
    unsigned int pingpongColorbuffers[2];
    glGenFramebuffers(2, pingpongFBO);
    glGenTextures(2, pingpongColorbuffers);
    for (unsigned int i = 0; i < 2; i++)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, pingpongFBO[i]);
        glBindTexture(GL_TEXTURE_2D, pingpongColorbuffers[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, SCR_WIDTH, SCR_HEIGHT, 0, GL_RGBA, GL_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); // we clamp to the edge as the blur filter would otherwise sample repeated texture values!
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, pingpongColorbuffers[i], 0);
        // also check if framebuffers are complete (no need for depth buffer)
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            std::cout << "Framebuffer not complete!" << std::endl;
    }

    unsigned int quadVAO, quadVBO;
    glGenVertexArrays(1, &quadVAO);
    glGenBuffers(1, &quadVBO);
    glBindVertexArray(quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));

    //skybox init

    unsigned int skyboxVAO, skyboxVBO;
    glGenVertexArrays(1, &skyboxVAO);
    glGenBuffers(1, &skyboxVBO);
    glBindVertexArray(skyboxVAO);
    glBindBuffer(GL_ARRAY_BUFFER, skyboxVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), &skyboxVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3*sizeof(float), (void*)0);

    unsigned int grassVAO, grassVBO;
    glGenVertexArrays(1, &grassVAO);
    glGenBuffers(1, &grassVBO);
    glBindVertexArray(grassVAO);
    glBindBuffer(GL_ARRAY_BUFFER, grassVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(grassVertices), grassVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glBindVertexArray(0);

    unsigned int grassTexture = loadTexture(FileSystem::getPath("resources/textures/grass/grass-min.png").c_str());
    unsigned int grassTextureSpec = loadTexture(FileSystem::getPath("resources/textures/grass/grass-min_specular.png").c_str());
    // load sky block textures
    std::vector<std::string> faces{
            "resources/textures/skybox/newSkyBox/right.jpg",
            "resources/textures/skybox/newSkyBox/left.jpg",
            "resources/textures/skybox/newSkyBox/top.jpg",
            "resources/textures/skybox/newSkyBox/bottom.jpg",
            "resources/textures/skybox/newSkyBox/front.jpg",
            "resources/textures/skybox/newSkyBox/back.jpg"
    };

    unsigned int cubemapTexture = loadCubemap(faces);

    // load models
    // -----------

    Model fieldModel("resources/objects/field/model.obj");
    fieldModel.SetShaderTextureNamePrefix("material.");

    Model tractorModel("resources/objects/tractor/Tractor_with_hydraulic_lifter_retopo2_SF.obj");
    tractorModel.SetShaderTextureNamePrefix("material.");

    Model tractor2Model("resources/objects/tractor2/New_holland_T7_Tractor_SF.obj");
    tractor2Model.SetShaderTextureNamePrefix("material.");

    Model cowModel("resources/objects/cow/cow.obj");
    cowModel.SetShaderTextureNamePrefix("material.");

    Model windmillModel("resources/objects/windmill/model.obj");
    windmillModel.SetShaderTextureNamePrefix("material.");

    Model houseModel("resources/objects/house/model.obj");
    houseModel.SetShaderTextureNamePrefix("material.");

    Model windmillMovModel("resources/objects/windmill_mov/windmill.obj");
    windmillMovModel.SetShaderTextureNamePrefix("material.");

    Model windmillStatModel("resources/objects/windmill_stat/windmill.obj");
    windmillStatModel.SetShaderTextureNamePrefix("material.");

    Model sunflowerModel("resources/objects/sunflower/sunflower.obj");
    sunflowerModel.SetShaderTextureNamePrefix("material.");

    Model ledModel("resources/objects/LED/LED_E.obj");
    ledModel.SetShaderTextureNamePrefix("material.");

    PointLight& pointLight = programState->pointLight;
    pointLight.position = glm::vec3(0.0f, 4.0, 12.0);
    pointLight.ambient = glm::vec3(0.1, 0.1, 0.1);
    pointLight.diffuse = glm::vec3(0.6, 0.6, 0.6);
    pointLight.specular = glm::vec3(1.0, 1.0, 1.0);

    pointLight.constant = 1.0f;
    pointLight.linear = 0.09f;
    pointLight.quadratic = 0.032f;

    // draw in wireframe
    //glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    glEnable(GL_DEPTH_TEST);
    // render loop
    // -----------
    double prevTime = 0.0;
    double currTime = 0.0;
    double timeDiff;
    unsigned int counter = 0;

    glm::vec3 grassPositions[] = {
            glm::vec3(-15.0f, -3.1f, 14.0f),
            glm::vec3(-26.0f, -3.1f, 11.0f),
            glm::vec3(-18.0f, -3.1f, 1.45f),
            glm::vec3(-12.0f, -3.1f, 22.0f),
            glm::vec3(-25.0f, -3.1f, 32.0f),
            glm::vec3(-38.0f, -3.1f, 23.0f)
    };

    float rotAngle = 0.0f;

    while (!glfwWindowShouldClose(window)) {

        currTime = glfwGetTime();
        timeDiff = currTime - prevTime;
        counter++;
        if(timeDiff >= 1.0 / 30.0){
            std::string FPS = std::to_string(1.0 / timeDiff * counter);
            std::string ms = std::to_string((timeDiff / counter) * 1000);
            std::string newTitle = FPS + " - FPS / " + ms + " - ms";
            glfwSetWindowTitle(window, newTitle.c_str());
            prevTime = currTime;
            counter = 0;
        }

        // per-frame time logic
        // --------------------
        float currentFrame = glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        // -----
        processInput(window);


        // render
        // ------
        //glClearColor(programState->clearColor.r, programState->clearColor.g, programState->clearColor.b, 1.0f);
        glBindFramebuffer(GL_FRAMEBUFFER, hdrFBO);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        blendingShader.use();
        blendingShader.setInt("texture1", 0);
        PointLight rotPointLight;

        ourShader.setVec3("pointLight.position", 9.1f, 0.0f, 14.0f);
        ourShader.setVec3("pointLight.ambient", 0.1f, 0.1f, 0.1f);
        ourShader.setVec3("pointLight.diffuse", 1.0f, 0.6f, 0.0f);
        ourShader.setVec3("pointLight.specular", 1.0f, 0.6f, 0.0f);
        ourShader.setFloat("pointLight.constant", 0.2f);
        ourShader.setFloat("pointLight.linear", 0.9f);
        ourShader.setFloat("pointLight.quadratic", 0.032f);
        ourShader.setVec3("viewPosition", programState->camera.Position);

        blendingShader.setVec3("dirLight.direction", -0.2f, -1.0f, 0.3f);
        blendingShader.setVec3("dirLight.ambient", 0.01f, 0.01f, 0.01f);
        blendingShader.setVec3("dirLight.diffuse", 0.2f, 0.2f, 0.2f);
        blendingShader.setVec3("dirLight.specular", 0.3f, 0.3f, 0.3f);
        
        glm::mat4 model = glm::mat4(1.0f);
        glm::mat4 projection = glm::perspective(glm::radians(programState->camera.Zoom), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);
        glm::mat4 view = programState->camera.GetViewMatrix();
        blendingShader.setMat4("projection", projection);
        blendingShader.setMat4("view", view);
        glBindVertexArray(grassVAO);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, grassTexture);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, grassTextureSpec);


        for (auto i : grassPositions)
        {
            float grassAngle = 0.0f;
            for(int j = 0; j < 6; j++) {
                model = glm::mat4(1.0f);
                model = glm::translate(model, i);
                model = glm::rotate(model, glm::radians(grassAngle), glm::vec3(0.0f, 1.0f, 0.0f));
                model = glm::translate(model, glm::vec3(-0.5f, 0.0f, 0.0f));
                blendingShader.setMat4("model", model);
                glDrawArrays(GL_TRIANGLES, 0, 6);
                grassAngle += 30.0f;
            }
        }
        glBindVertexArray(0);
        glEnable(GL_CULL_FACE);

        ourShader.use();
        ourShader.setMat4("projection", projection);
        ourShader.setMat4("view", view);

        PointLight pointLight1, pointLight2;

        ourShader.setFloat("material.shininess", 32.0f);
        ourShader.setVec3("rotPointLight.position", 9.1f, -0.22f, 14.0f);
        ourShader.setVec3("rotPointLight.direction", glm::vec3(0.0f, 0.0f, 1.0f));
        ourShader.setVec3("rotPointLight.ambient", 0.1f, 0.1f, 0.1f);
        ourShader.setVec3("rotPointLight.diffuse", 1.0f, 0.6f, 0.0f);
        ourShader.setVec3("rotPointLight.specular", 1.0f, 0.6f, 0.0f);
        ourShader.setFloat("rotPointLight.constant", 0.1f);
        ourShader.setFloat("rotPointLight.linear", 0.9f);
        ourShader.setFloat("rotPointLight.quadratic", 0.032f);
        ourShader.setFloat("rotPointLight.cutOff", glm::cos(glm::radians(0.0f)));
        ourShader.setFloat("rotPointLight.outerCutOff", glm::cos(glm::radians(rotAngle)));
        ourShader.setVec3("viewPosition", programState->camera.Position);

        ourShader.setVec3("rotPointLight1.position", 9.1f, -0.22f, 14.0f);
        ourShader.setVec3("rotPointLight1.direction", glm::vec3(0.0f, 0.0f, -1.0f));
        ourShader.setVec3("rotPointLight1.ambient", 0.1f, 0.1f, 0.1f);
        ourShader.setVec3("rotPointLight1.diffuse", 1.0f, 0.6f, 0.0f);
        ourShader.setVec3("rotPointLight1.specular", 1.0f, 0.6f, 0.0f);
        ourShader.setFloat("rotPointLight1.constant", 0.1f);
        ourShader.setFloat("rotPointLight1.linear", 0.9f);
        ourShader.setFloat("rotPointLight1.quadratic", 0.032f);
        ourShader.setFloat("rotPointLight1.cutOff", glm::cos(glm::radians(0.0f)));
        ourShader.setFloat("rotPointLight1.outerCutOff", glm::cos(glm::radians(180.0f + rotAngle)));
        ourShader.setVec3("viewPosition", programState->camera.Position);

        ourShader.setVec3("pointLight1.position", 10.1f, -1.87f, 17.3f);
        ourShader.setVec3("pointLight1.ambient", pointLight.ambient);
        ourShader.setVec3("pointLight1.diffuse", pointLight.diffuse);
        ourShader.setVec3("pointLight1.specular", pointLight.specular);
        ourShader.setFloat("pointLight1.constant", 0.45f);
        ourShader.setFloat("pointLight1.linear", 0.85f);
        ourShader.setFloat("pointLight1.quadratic", 0.032f);
        ourShader.setVec3("viewPosition", programState->camera.Position);

        ourShader.setVec3("pointLight2.position", 9.6f, -1.87f, 17.3f);
        ourShader.setVec3("pointLight2.ambient", pointLight.ambient);
        ourShader.setVec3("pointLight2.diffuse", pointLight.diffuse);
        ourShader.setVec3("pointLight2.specular", pointLight.specular);
        ourShader.setFloat("pointLight2.constant", 0.45f);
        ourShader.setFloat("pointLight2.linear", 0.85f);
        ourShader.setFloat("pointLight2.quadratic", 0.032f);
        ourShader.setVec3("viewPosition", programState->camera.Position);

        ourShader.setVec3("dirLight.direction", -0.2f, -1.0f, 0.3f);
        ourShader.setVec3("dirLight.ambient", 0.01f, 0.01f, 0.01f);
        ourShader.setVec3("dirLight.diffuse", 0.2f, 0.2f, 0.2f);
        ourShader.setVec3("dirLight.specular", 0.3f, 0.3f, 0.3f);


        SpotLight spotLight1;
        SpotLight spotLight2;


        ourShader.setVec3("spotLight1.position", 9.9f, -2.3f, 14.5f);
        ourShader.setVec3("spotLight1.direction", 0.0f, -0.07f, 1.0f);
        ourShader.setVec3("spotLight1.ambient", 0.0f, 0.0f, 0.0f);
        ourShader.setVec3("spotLight1.diffuse", 1.0f, 1.0f, 1.0f);
        ourShader.setVec3("spotLight1.specular", 1.0f, 1.0f, 1.0f);
        ourShader.setFloat("spotLight1.constant", 1.0f);
        ourShader.setFloat("spotLight1.linear", 0.09);
        ourShader.setFloat("spotLight1.quadratic", 0.032);
        ourShader.setFloat("spotLight1.cutOff", glm::cos(glm::radians(19.875f)));
        ourShader.setFloat("spotLight1.outerCutOff", glm::cos(glm::radians(21.0f)));

        ourShader.setVec3("spotLight2.position", 9.5f, -2.3f, 14.5f);
        ourShader.setVec3("spotLight2.direction", 0.0f, -0.07f, 1.0f);
        ourShader.setVec3("spotLight2.ambient", 0.0f, 0.0f, 0.0f);
        ourShader.setVec3("spotLight2.diffuse", 1.0f, 1.0f, 1.0f);
        ourShader.setVec3("spotLight2.specular", 1.0f, 1.0f, 1.0f);
        ourShader.setFloat("spotLight2.constant", 1.0f);
        ourShader.setFloat("spotLight2.linear", 0.09);
        ourShader.setFloat("spotLight2.quadratic", 0.032);
        ourShader.setFloat("spotLight2.cutOff", glm::cos(glm::radians(19.875f)));
        ourShader.setFloat("spotLight2.outerCutOff", glm::cos(glm::radians(21.0f)));


        ourShader.setVec3("pointLight3.position", 10.5f, -1.0f, 12.6f);
        ourShader.setVec3("pointLight3.ambient", glm::vec3(0.1f));
        ourShader.setVec3("pointLight3.diffuse", glm::vec3(0.73f, 0.1176f, 0.0627f));
        ourShader.setVec3("pointLight3.specular", glm::vec3(0.73f, 0.1176f, 0.0627f));
        ourShader.setFloat("pointLight3.constant", 0.3f);
        ourShader.setFloat("pointLight3.linear", 0.85f);
        ourShader.setFloat("pointLight3.quadratic", 0.032f);
        ourShader.setVec3("viewPosition", programState->camera.Position);

        ourShader.setVec3("pointLight4.position", 8.8f, -1.0f, 12.6f);
        ourShader.setVec3("pointLight4.ambient", glm::vec3(0.1f));
        ourShader.setVec3("pointLight4.diffuse", glm::vec3(0.73f, 0.1176f, 0.0627f));
        ourShader.setVec3("pointLight4.specular", glm::vec3(0.73f, 0.1176f, 0.0627f));
        ourShader.setFloat("pointLight4.constant", 0.3f);
        ourShader.setFloat("pointLight4.linear", 0.85f);
        ourShader.setFloat("pointLight4.quadratic", 0.032f);
        ourShader.setVec3("viewPosition", programState->camera.Position);

        // render the loaded model
        model = glm::mat4(1.0f);
        model = glm::translate(model,glm::vec3(0.0f,0.0f,0.0f)); // translate it down so it's at the center of the scene
        model = glm::rotate(model, glm::radians(4.675f), glm::vec3(0.0f, 1.0f, 0.0f));
        model = glm::scale(model, glm::vec3(1.0f));    // it's a bit too big for our scene, so scale it down
        ourShader.setMat4("model", model);
        fieldModel.Draw(ourShader);

        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(0.0f, -3.6f,12.0f));
        model = glm::scale(model, glm::vec3(0.4f));
        ourShader.setMat4("model", model);
        tractorModel.Draw(ourShader);

        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(9.0f, -3.6f, 12.0f));
        model = glm::scale(model, glm::vec3(1.0f));
        ourShader.setMat4("model", model);
        tractor2Model.Draw(ourShader);

        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(-29.0f, -6.3f, 26.0f));
        model = glm::rotate(model, glm::radians(-0.4f), glm::vec3(1.0f, 0.0f, 0.0f));
        model = glm::scale(model, glm::vec3(0.5f));
        ourShader.setMat4("model", model);
        houseModel.Draw(ourShader);

        glDisable(GL_CULL_FACE);

        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(9.1f, -0.42f, 14.0f));
        model = glm::rotate(model, glm::radians(float(rotAngle)), glm::vec3(0.0f, 1.0f, 0.0f));
        rotAngle += 15.0f;
        model = glm::scale(model, glm::vec3(0.1f));
        ourShader.setMat4("model", model);
        ledModel.Draw(ourShader);

        glEnable(GL_CULL_FACE);

        std::vector<glm::vec3> cowPositions = {
                glm::vec3(-12.0f, -3.56f, 8.1f),
                glm::vec3(-22.0f, -3.58f, 12.0f)
        };

        for(int i = 0; i < cowPositions.size(); i++){
            model = glm::mat4(1.0f);
            model = glm::translate(model, cowPositions[i]);
            model = glm::rotate(model, glm::radians(95.0f * float(i)), glm::vec3(0.0f, 1.0f, 0.0f));
            model = glm::scale(model, glm::vec3(0.2f));
            ourShader.setMat4("model", model);
            cowModel.Draw(ourShader);
        }

        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(21.0f, -3.8f, 10.0f));
        model = glm::rotate(model, glm::radians(170.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        model = glm::scale(model, glm::vec3(0.5f));
        ourShader.setMat4("model", model);
        windmillModel.Draw(ourShader);

        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(-27.225f, 2.425f, 3.725f));
        model = glm::rotate(model, glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        model = glm::rotate(model, glm::radians(-7.0f), glm::vec3(1.0f, 0.0f, 0.0f));
        model = glm::rotate(model, glm::radians((float)glfwGetTime()*10), glm::vec3(0.0f, 0.0f, 1.0f));
        model = glm::scale(model, glm::vec3(1.0f));
        ourShader.setMat4("model", model);
        windmillMovModel.Draw(ourShader);

        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(-30.0f, -4.6f, 4.0f));
        model = glm::rotate(model, glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        model = glm::scale(model, glm::vec3(1.0f));
        ourShader.setMat4("model", model);
        windmillStatModel.Draw(ourShader);

        glm::vec3 positionOfSunflower = glm::vec3(-29.0f, -4.2f, -9.0f);
        float zOfSunflowerRow = 0.0f;
        for(int i = 0; i < 8; i++){
            for(int j = 0; j < 45; j++){
                model = glm::mat4(1.0f);
                model = glm::translate(model, positionOfSunflower + glm::vec3(float(j) * 1.5f, 0.0f, zOfSunflowerRow));
                model = glm::scale(model, glm::vec3(0.02f));
                ourShader.setMat4("model", model);
                sunflowerModel.Draw(ourShader);
            }
            zOfSunflowerRow -= 2.5f;
        }

        glDepthFunc(GL_LEQUAL);
        skyboxShader.use();
        skyboxShader.setInt("skybox", 0);

        skyboxShader.setMat4("view", glm::mat4(glm::mat3(view)));
        skyboxShader.setMat4("projection", projection);
        glBindVertexArray(skyboxVAO);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_CUBE_MAP, cubemapTexture);
        glDrawArrays(GL_TRIANGLES, 0, 36);

        glDepthFunc(GL_LESS);

        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        bool horizontal = true, first_iteration = true;
        unsigned int amount = 10;
        blurShader.use();
        for (unsigned int i = 0; i < amount; i++)
        {
            glBindFramebuffer(GL_FRAMEBUFFER, pingpongFBO[horizontal]);
            blurShader.setInt("horizontal", horizontal);
            glBindTexture(GL_TEXTURE_2D, first_iteration ? colorBuffers[1] : pingpongColorbuffers[!horizontal]);  // bind texture of other framebuffer (or scene if first iteration)
            glBindVertexArray(quadVAO);
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
            glBindVertexArray(0);
            horizontal = !horizontal;
            if (first_iteration)
                first_iteration = false;
        }
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        bloomShader.use();
        glActiveTexture(GL_TEXTURE0);

        glBindTexture(GL_TEXTURE_2D, colorBuffers[0]);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, pingpongColorbuffers[!horizontal]);
        bloomShader.setInt("bloom", bBloom);
        bloomShader.setFloat("exposure", 0.1f);


        glBindVertexArray(quadVAO);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        glBindVertexArray(0);


        if (programState->ImGuiEnabled)
            DrawImGui(programState);



        // glfw: swap buffers and poll IO events (keys pressed/released, mouse moved etc.)
        // -------------------------------------------------------------------------------
        glfwSwapBuffers(window);
        glfwPollEvents();
        glDisable(GL_CULL_FACE);
    }

    programState->SaveToFile("resources/program_state.txt");
    delete programState;
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glDeleteVertexArrays(1, &skyboxVAO);
    glDeleteBuffers(1, &skyboxVAO);
    glDeleteVertexArrays(1, &grassVAO);
    glDeleteBuffers(1, &grassVAO);
    glDeleteVertexArrays(1, &quadVAO);
    glDeleteBuffers(1, &quadVAO);
    // glfw: terminate, clearing all previously allocated GLFW resources.
    // ------------------------------------------------------------------
    glfwTerminate();
    return 0;
}

// process all input: query GLFW whether relevant keys are pressed/released this frame and react accordingly
// ---------------------------------------------------------------------------------------------------------
void processInput(GLFWwindow *window) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        programState->camera.ProcessKeyboard(FORWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        programState->camera.ProcessKeyboard(BACKWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        programState->camera.ProcessKeyboard(LEFT, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        programState->camera.ProcessKeyboard(RIGHT, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
        programState->camera.ProcessKeyboard(UP, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_X) == GLFW_PRESS)
        programState->camera.ProcessKeyboard(DOWN, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_H) == GLFW_PRESS){
        if(bBloom){
            bBloom = false;
        }
        else{
            bBloom = true;
        }
    }
}

// glfw: whenever the window size changed (by OS or user resize) this callback function executes
// ---------------------------------------------------------------------------------------------
void framebuffer_size_callback(GLFWwindow *window, int width, int height) {
    // make sure the viewport matches the new window dimensions; note that width and
    // height will be significantly larger than specified on retina displays.
    glViewport(0, 0, width, height);
}

// glfw: whenever the mouse moves, this callback is called
// -------------------------------------------------------
void mouse_callback(GLFWwindow *window, double xpos, double ypos) {
    if (firstMouse) {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }

    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos; // reversed since y-coordinates go from bottom to top

    lastX = xpos;
    lastY = ypos;

    if (programState->CameraMouseMovementUpdateEnabled)
        programState->camera.ProcessMouseMovement(xoffset, yoffset);
}

// glfw: whenever the mouse scroll wheel scrolls, this callback is called
// ----------------------------------------------------------------------
void scroll_callback(GLFWwindow *window, double xoffset, double yoffset) {
    programState->camera.ProcessMouseScroll(yoffset);
}

void DrawImGui(ProgramState *programState) {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();


    {
        static float f = 0.0f;
        ImGui::Begin("Hello window");
        ImGui::Text("Hello text");
        ImGui::SliderFloat("Float slider", &f, 0.0, 1.0);
        ImGui::ColorEdit3("Background color", (float *) &programState->clearColor);
        ImGui::DragFloat3("Backpack position", (float*)&programState->backpackPosition, 0.025);
        ImGui::DragFloat("Backpack scale", &programState->backpackScale, 0.025, -100.0, 100.0);

        ImGui::DragFloat("pointLight.constant", &programState->pointLight.constant, 0.05, 0.0, 1.0);
        ImGui::DragFloat("pointLight.linear", &programState->pointLight.linear, 0.05, 0.0, 1.0);
        ImGui::DragFloat("pointLight.quadratic", &programState->pointLight.quadratic, 0.05, 0.0, 1.0);
        ImGui::End();
    }

    {
        ImGui::Begin("Camera info");
        const Camera& c = programState->camera;
        ImGui::Text("Camera position: (%f, %f, %f)", c.Position.x, c.Position.y, c.Position.z);
        ImGui::Text("(Yaw, Pitch): (%f, %f)", c.Yaw, c.Pitch);
        ImGui::Text("Camera front: (%f, %f, %f)", c.Front.x, c.Front.y, c.Front.z);
        ImGui::Checkbox("Camera mouse update", &programState->CameraMouseMovementUpdateEnabled);
        ImGui::End();
    }

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void key_callback(GLFWwindow *window, int key, int scancode, int action, int mods) {
    if (key == GLFW_KEY_F1 && action == GLFW_PRESS) {
        programState->ImGuiEnabled = !programState->ImGuiEnabled;
        if (programState->ImGuiEnabled) {
            programState->CameraMouseMovementUpdateEnabled = false;
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        } else {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        }
    }
}

unsigned int loadTexture(char const * path)
{
    unsigned int textureID;
    glGenTextures(1, &textureID);

    int width, height, nrComponents;
    unsigned char *data = stbi_load(path, &width, &height, &nrComponents, 0);
    if (data)
    {
        GLenum format;
        if (nrComponents == 1)
            format = GL_RED;
        else if (nrComponents == 3)
            format = GL_RGB;
        else if (nrComponents == 4)
            format = GL_RGBA;

        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, format == GL_RGBA ? GL_CLAMP_TO_EDGE : GL_REPEAT); // for this tutorial: use GL_CLAMP_TO_EDGE to prevent semi-transparent borders. Due to interpolation it takes texels from next repeat
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, format == GL_RGBA ? GL_CLAMP_TO_EDGE : GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        stbi_image_free(data);
    }
    else
    {
        std::cout << "Texture failed to load at path: " << path << std::endl;
        stbi_image_free(data);
    }

    return textureID;
}


unsigned int loadCubemap(std::vector<std::string>& faces) {
    unsigned int textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_CUBE_MAP, textureID);

    int width, height, nrChannels;
    unsigned char *data;

    for (int i = 0; i < faces.size(); i++) {
        data = stbi_load(faces[i].c_str(), &width, &height, &nrChannels, 0);
        if (data) {
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE,
                         data);
        } else {
            std::cerr << "Failed to load cube map texture face\n";
            return -1;
        }
        stbi_image_free(data);
    }

    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    return textureID;
}
