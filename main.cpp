#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/type_ptr.hpp"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <vector>
#include <string>

#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

struct Material {
    glm::vec3 ambient;
    glm::vec3 diffuse;
    glm::vec3 specular;
    float shininess;
};

struct SceneConfig {
    const char* name;
    Material material;
    glm::vec3 lightPos;
    glm::vec3 lightColor;
    glm::vec3 backgroundColor;
    glm::vec3 cameraPos;
    float fov;
    float modelScale;
    float rotationSpeed;
};

int activeConfig = 0;
bool keyWasPressed[4] = { false, false, false, false };
bool useMouseLight = true;
glm::vec3 mouseLightOffset(0.0f);

// Shader Sumber
const char* vertexShaderSource = "#version 330 core\n"
    "layout (location = 0) in vec3 aPos;\n"
    "layout (location = 1) in vec3 aNormal;\n"
    "out vec3 FragPos;\n"
    "out vec3 Normal;\n"
    "uniform mat4 model; uniform mat4 view; uniform mat4 projection;\n"
    "void main() {\n"
    "   FragPos = vec3(model * vec4(aPos, 1.0));\n"
    "   Normal = mat3(transpose(inverse(model))) * aNormal;\n"
    "   gl_Position = projection * view * vec4(FragPos, 1.0);\n"
    "}\0";

const char* fragmentShaderSource = "#version 330 core\n"
    "out vec4 FragColor;\n"
    "in vec3 Normal; in vec3 FragPos;\n"
    "struct Material {\n"
    "   vec3 ambient;\n"
    "   vec3 diffuse;\n"
    "   vec3 specular;\n"
    "   float shininess;\n"
    "};\n"
    "uniform Material material;\n"
    "uniform vec3 lightPos;\n"
    "uniform vec3 viewPos;\n"
    "uniform vec3 lightColor;\n"
    "void main() {\n"
    "   vec3 norm = normalize(Normal);\n"
    "   vec3 ambient = material.ambient * lightColor;\n"
    "   vec3 lightDir = normalize(lightPos - FragPos);\n"
    "   float diff = max(dot(norm, lightDir), 0.0);\n"
    "   vec3 diffuse = material.diffuse * diff * lightColor;\n"
    "   vec3 viewDir = normalize(viewPos - FragPos);\n"
    "   vec3 reflectDir = reflect(-lightDir, norm);\n"
    "   float spec = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess);\n"
    "   vec3 specular = material.specular * spec * lightColor;\n"
    "   vec3 result = ambient + diffuse + specular;\n"
    "   FragColor = vec4(result, 1.0);\n"
    "}\n\0";

const SceneConfig sceneConfigs[] = {
    {
        "Putih indoor - glossy",
        { glm::vec3(0.20f, 0.20f, 0.19f), glm::vec3(0.88f, 0.86f, 0.78f), glm::vec3(0.55f, 0.55f, 0.50f), 64.0f },
        glm::vec3(3.0f, 5.0f, 4.0f),
        glm::vec3(1.0f, 1.0f, 1.0f),
        glm::vec3(0.08f, 0.09f, 0.10f),
        glm::vec3(0.0f, 1.0f, 5.0f),
        42.0f,
        0.085f,
        0.8f
    },
    {
        "Kuning latihan - warm",
        { glm::vec3(0.20f, 0.16f, 0.08f), glm::vec3(0.95f, 0.72f, 0.20f), glm::vec3(0.32f, 0.28f, 0.18f), 32.0f },
        glm::vec3(-3.5f, 4.5f, 3.0f),
        glm::vec3(1.0f, 0.90f, 0.72f),
        glm::vec3(0.14f, 0.11f, 0.07f),
        glm::vec3(0.8f, 1.2f, 5.8f),
        48.0f,
        0.082f,
        0.55f
    },
    {
        "Putih soft - studio",
        { glm::vec3(0.18f, 0.19f, 0.20f), glm::vec3(0.82f, 0.84f, 0.82f), glm::vec3(0.40f, 0.43f, 0.42f), 24.0f },
        glm::vec3(2.0f, 6.0f, -3.5f),
        glm::vec3(0.82f, 0.90f, 1.0f),
        glm::vec3(0.04f, 0.06f, 0.11f),
        glm::vec3(-0.7f, 1.1f, 6.3f),
        36.0f,
        0.088f,
        1.15f
    }
};

void checkShaderCompile(unsigned int shader, const char* shaderName) {
    int success;
    char infoLog[1024];
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(shader, 1024, NULL, infoLog);
        std::cout << "Shader " << shaderName << " gagal compile:\n" << infoLog << std::endl;
    }
}

void checkProgramLink(unsigned int program) {
    int success;
    char infoLog[1024];
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(program, 1024, NULL, infoLog);
        std::cout << "Shader program gagal link:\n" << infoLog << std::endl;
    }
}

void processInput(GLFWwindow* window) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, true);
    }

    const int keys[] = { GLFW_KEY_1, GLFW_KEY_2, GLFW_KEY_3 };
    for (int i = 0; i < 3; ++i) {
        bool isPressed = glfwGetKey(window, keys[i]) == GLFW_PRESS;
        if (isPressed && !keyWasPressed[i]) {
            activeConfig = i;
            std::cout << "Konfigurasi aktif: " << sceneConfigs[activeConfig].name << std::endl;
        }
        keyWasPressed[i] = isPressed;
    }

    bool lPressed = glfwGetKey(window, GLFW_KEY_L) == GLFW_PRESS;
    if (lPressed && !keyWasPressed[3]) {
        useMouseLight = !useMouseLight;
        std::cout << "Mouse lighting: " << (useMouseLight ? "aktif" : "nonaktif") << std::endl;
    }
    keyWasPressed[3] = lPressed;
}

void updateMouseLight(GLFWwindow* window) {
    int width, height;
    double mouseX, mouseY;
    glfwGetFramebufferSize(window, &width, &height);
    glfwGetCursorPos(window, &mouseX, &mouseY);

    if (width <= 0 || height <= 0) return;

    float normalizedX = static_cast<float>((mouseX / width) * 2.0 - 1.0);
    float normalizedY = static_cast<float>(1.0 - (mouseY / height) * 2.0);

    mouseLightOffset = glm::vec3(normalizedX * 5.0f, normalizedY * 4.0f, 0.0f);
}

bool loadModel(const char* path, std::vector<float>& vertices) {
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;
    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, path)) {
        if (!warn.empty()) std::cout << "Warning OBJ: " << warn << std::endl;
        if (!err.empty()) std::cout << "Error OBJ: " << err << std::endl;
        return false;
    }
    if (!warn.empty()) std::cout << "Warning OBJ: " << warn << std::endl;
    for (const auto& shape : shapes) {
        for (const auto& index : shape.mesh.indices) {
            vertices.push_back(attrib.vertices[3 * index.vertex_index + 0]);
            vertices.push_back(attrib.vertices[3 * index.vertex_index + 1]);
            vertices.push_back(attrib.vertices[3 * index.vertex_index + 2]);
            if (index.normal_index >= 0) {
                vertices.push_back(attrib.normals[3 * index.normal_index + 0]);
                vertices.push_back(attrib.normals[3 * index.normal_index + 1]);
                vertices.push_back(attrib.normals[3 * index.normal_index + 2]);
            } else {
                vertices.push_back(0.0f); vertices.push_back(1.0f); vertices.push_back(0.0f);
            }
        }
    }
    return true;
}

int main() {
    // 1. Inisialisasi Window DULUAN
    if (!glfwInit()) return -1;
    GLFWwindow* window = glfwCreateWindow(800, 600, "Tugas Projek OpenGL", NULL, NULL);
    if (!window) { glfwTerminate(); return -1; }
    glfwMakeContextCurrent(window);

    // 2. Load GLAD (Agar fungsi OpenGL bisa dipakai)
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) return -1;

    glfwSetFramebufferSizeCallback(window, [](GLFWwindow*, int width, int height) {
        glViewport(0, 0, width, height);
    });
    glEnable(GL_DEPTH_TEST); // Biar objek 3D gak tumpang tindih ngaco

    // 3. Load Model
    std::vector<float> modelVertices;
    if (!loadModel("models/volleyball.obj", modelVertices)) {
        std::cout << "Gagal load model! Cek path folder models/" << std::endl;
        glfwTerminate();
        return -1;
    } else {
        std::cout << "Model Berhasil Dimuat! Vertices: " << modelVertices.size() / 6 << std::endl;
    }
    std::cout << "Tekan 1/2/3 untuk mengganti konfigurasi material, lampu, kamera, dan projection." << std::endl;
    std::cout << "Gerakkan mouse untuk mengatur arah cahaya. Tekan L untuk toggle mouse lighting." << std::endl;

    // 4. Compile Shader
    unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
    glCompileShader(vertexShader);
    checkShaderCompile(vertexShader, "vertex");
    unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
    glCompileShader(fragmentShader);
    checkShaderCompile(fragmentShader, "fragment");
    unsigned int shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);
    checkProgramLink(shaderProgram);
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    // 5. Setup VAO & VBO (Harus sesudah GLAD load)
    unsigned int VBO, VAO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, modelVertices.size() * sizeof(float), modelVertices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // 6. Loop Render
    while (!glfwWindowShouldClose(window)) {
        processInput(window);
        updateMouseLight(window);

        const SceneConfig& config = sceneConfigs[activeConfig];

        glClearColor(config.backgroundColor.r, config.backgroundColor.g, config.backgroundColor.b, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(shaderProgram);

        int width, height;
        glfwGetFramebufferSize(window, &width, &height);
        float aspect = height > 0 ? static_cast<float>(width) / static_cast<float>(height) : 1.0f;

        // Matriks 3D
        glm::mat4 projection = glm::perspective(glm::radians(config.fov), aspect, 0.1f, 1000.0f);
        glm::mat4 view = glm::lookAt(config.cameraPos, glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::rotate(model, (float)glfwGetTime() * config.rotationSpeed, glm::vec3(0.15f, 1.0f, 0.1f)); // Muter otomatis
        model = glm::scale(model, glm::vec3(config.modelScale));

        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "view"), 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(model));

        // Pencahayaan
        glm::vec3 lightPos = useMouseLight ? config.lightPos + mouseLightOffset : config.lightPos;
        glUniform3fv(glGetUniformLocation(shaderProgram, "lightPos"), 1, glm::value_ptr(lightPos));
        glUniform3fv(glGetUniformLocation(shaderProgram, "viewPos"), 1, glm::value_ptr(config.cameraPos));
        glUniform3fv(glGetUniformLocation(shaderProgram, "lightColor"), 1, glm::value_ptr(config.lightColor));
        glUniform3fv(glGetUniformLocation(shaderProgram, "material.ambient"), 1, glm::value_ptr(config.material.ambient));
        glUniform3fv(glGetUniformLocation(shaderProgram, "material.diffuse"), 1, glm::value_ptr(config.material.diffuse));
        glUniform3fv(glGetUniformLocation(shaderProgram, "material.specular"), 1, glm::value_ptr(config.material.specular));
        glUniform1f(glGetUniformLocation(shaderProgram, "material.shininess"), config.material.shininess);
        
        glBindVertexArray(VAO);
        glDrawArrays(GL_TRIANGLES, 0, modelVertices.size() / 6);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteProgram(shaderProgram);
    glfwTerminate();
    return 0;
}
