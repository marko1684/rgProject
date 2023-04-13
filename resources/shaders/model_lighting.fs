#version 330 core
out vec4 FragColor;

struct PointLight {
    vec3 position;

    vec3 specular;
    vec3 diffuse;
    vec3 ambient;

    float constant;
    float linear;
    float quadratic;
};

struct DirLight {
    vec3 direction;

    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};
uniform DirLight dirLight;

struct SpotLight {
    vec3 position;
    vec3 direction;
    float cutOff;
    float outerCutOff;

    float constant;
    float linear;
    float quadratic;

    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};

uniform SpotLight spotLight1;
uniform SpotLight spotLight2;
// uniform SpotLight spotLight3;
// uniform SpotLight spotLight4;


struct Material {
    sampler2D texture_diffuse1;
    sampler2D texture_specular1;

    float shininess;
};

in vec2 TexCoords;
in vec3 Normal;
in vec3 FragPos;

uniform PointLight pointLight1;
uniform PointLight pointLight2;
uniform SpotLight rotPointLight;
uniform SpotLight rotPointLight1;
uniform Material material;

uniform vec3 viewPosition;
// calculates the color when using a point light.
vec3 CalcPointLight(PointLight light, vec3 normal, vec3 fragPos, vec3 viewDir)
{
    vec3 lightDir = normalize(light.position - fragPos);
    // diffuse shading
    float diff = max(dot(normal, lightDir), 0.0);
    // specular shading
    // vec3 reflectDir = reflect(-lightDir, normal);
    vec3 halfwayDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(viewDir, halfwayDir), 0.0), material.shininess);
    // attenuation
    float distance = length(light.position - fragPos);
    float attenuation = 1.0 / (light.constant + light.linear * distance + light.quadratic * (distance * distance));
    // combine results
    vec3 ambient = light.ambient * vec3(texture(material.texture_diffuse1, TexCoords));
    vec3 diffuse = light.diffuse * diff * vec3(texture(material.texture_diffuse1, TexCoords));
    vec3 specular = light.specular * spec * vec3(texture(material.texture_specular1, TexCoords).xxx);

    ambient *= attenuation;
    diffuse *= attenuation;
    specular *= attenuation;
    return (ambient + diffuse + specular);
}

vec3 calcSpotLight(SpotLight light, vec3 normal, vec3 fragPos, vec3 viewDir)
 {
     vec3 lightDir = normalize(light.position - fragPos);
     // diffuse shading
     float diff = max(dot(normal, lightDir), 0.0);
     // specular shading
     vec3 halfwayDir = normalize(lightDir + viewDir);
     float spec = pow(max(dot(normal, halfwayDir), 0.0), material.shininess);
     // attenuation
     float distance = length(light.position - fragPos);
     float attenuation = 1.0 / (light.constant + light.linear * distance + light.quadratic * (distance * distance));
     // spotlight intensity
     float theta = dot(lightDir, normalize(-light.direction));
     float epsilon = light.cutOff - light.outerCutOff;
     float intensity = clamp((theta - light.outerCutOff) / epsilon, 0.0, 1.0);
     // combine results
     vec3 ambient = light.ambient * vec3(texture(material.texture_diffuse1, TexCoords));
     vec3 diffuse = light.diffuse * diff * vec3(texture(material.texture_diffuse1, TexCoords));
     vec3 specular = light.specular * spec * vec3(texture(material.texture_specular1, TexCoords));
     ambient *= attenuation * intensity;
     diffuse *= attenuation * intensity;
     specular *= attenuation * intensity;
     return (ambient + diffuse + specular);
 }

vec3 calcDirectionalLight(DirLight light, vec3 normal, vec3 viewDir){
    vec3 lightDir = normalize(-light.direction);
    // diffuse shading
    float diff = max(dot(normal, lightDir), 0.0);
    // specular shading
    vec3 reflectDir = reflect(-lightDir, normal);
    vec3 halfwayDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(viewDir, halfwayDir), 0.0), material.shininess);
    // combine results
    vec3 ambient  = light.ambient  * vec3(texture(material.texture_diffuse1, TexCoords));
    vec3 diffuse  = light.diffuse  * diff * vec3(texture(material.texture_diffuse1, TexCoords));
    vec3 specular = light.specular * spec * vec3(texture(material.texture_specular1, TexCoords));
    return (ambient + diffuse + specular);
}



void main()
{
    vec3 normal = normalize(Normal);
    vec3 viewDir = normalize(viewPosition - FragPos);
    vec3 result = CalcPointLight(pointLight1, normal, FragPos, viewDir);
    result += CalcPointLight(pointLight2, normal, FragPos, viewDir);
    result += calcSpotLight(rotPointLight, normal, FragPos, viewDir);
    result += calcSpotLight(rotPointLight1, normal, FragPos, viewDir);
    result += calcDirectionalLight(dirLight, normal, viewDir);
    result += calcSpotLight(spotLight1, normal, FragPos, viewDir);
    result += calcSpotLight(spotLight2, normal, FragPos, viewDir);

    FragColor = vec4(result, 1.0);
}