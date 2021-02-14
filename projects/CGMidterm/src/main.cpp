//Just a simple handler for simple initialization stuffs
#include "Utilities/BackendHandler.h"

#include <filesystem>
#include <json.hpp>
#include <fstream>

#include <Texture2D.h>
#include <Texture2DData.h>
#include <MeshBuilder.h>
#include <MeshFactory.h>
#include <NotObjLoader.h>
#include <ObjLoader.h>
#include <VertexTypes.h>
#include <ShaderMaterial.h>
#include <RendererComponent.h>
#include <TextureCubeMap.h>
#include <TextureCubeMapData.h>

#include <Timing.h>
#include <GameObjectTag.h>
#include <InputHelpers.h>

#include <IBehaviour.h>
#include <CameraControlBehaviour.h>
#include <FollowPathBehaviour.h>
#include <SimpleMoveBehaviour.h>

int main() {
	int frameIx = 0;
	float fpsBuffer[128];
	float minFps, maxFps, avgFps;
	int selectedVao = 0; // select cube by default
	std::vector<GameObject> controllables;

	BackendHandler::InitAll();

	// Let OpenGL know that we want debug output, and route it to our handler function
	glEnable(GL_DEBUG_OUTPUT);
	glDebugMessageCallback(BackendHandler::GlDebugMessage, nullptr);

	// Enable texturing
	glEnable(GL_TEXTURE_2D);

	// Push another scope so most memory should be freed *before* we exit the app
	{
		#pragma region Shader and ImGui
		Shader::sptr passthroughShader = Shader::Create();
		passthroughShader->LoadShaderPartFromFile("shaders/passthrough_vert.glsl", GL_VERTEX_SHADER);
		passthroughShader->LoadShaderPartFromFile("shaders/passthrough_frag.glsl", GL_FRAGMENT_SHADER);
		passthroughShader->Link();

		// Load our shaders
		Shader::sptr shader = Shader::Create();
		shader->LoadShaderPartFromFile("shaders/vertex_shader.glsl", GL_VERTEX_SHADER);
		shader->LoadShaderPartFromFile("shaders/frag_blinn_phong_textured.glsl", GL_FRAGMENT_SHADER);
		shader->Link();

		glm::vec3 lightPos = glm::vec3(0.0f, 0.0f, 5.0f);
		glm::vec3 lightCol = glm::vec3(0.9f, 0.85f, 0.5f);
		float     lightAmbientPow = 0.5f;
		float     lightSpecularPow = 1.0f;
		glm::vec3 ambientCol = glm::vec3(1.0f);
		float     ambientPow = 0.1;
		float     lightLinearFalloff = 0.09f;
		float     lightQuadraticFalloff = 0.032f;
		float	  Threshold = 0.35f;
		float	  Strength = 2.0f;
		float	  Blur = 0.0f;
		int		  NoLight = 0;
		int		  Ambient = 0;
		int		  Specular = 0;
		int		  AmbientSpecular = 0;
		int		  AmbientSpecularBloom = 0;
		int		  Textures = 1;
		bool	  OnOff = true;
		
		// These are our application / scene level uniforms that don't necessarily update
		// every frame
		shader->SetUniform("u_LightPos", lightPos);
		shader->SetUniform("u_LightCol", lightCol);
		shader->SetUniform("u_AmbientLightStrength", lightAmbientPow);
		shader->SetUniform("u_SpecularLightStrength", lightSpecularPow);
		shader->SetUniform("u_AmbientCol", ambientCol);
		shader->SetUniform("u_AmbientStrength", ambientPow);
		shader->SetUniform("u_LightAttenuationConstant", 1.0f);
		shader->SetUniform("u_LightAttenuationLinear", lightLinearFalloff);
		shader->SetUniform("u_LightAttenuationQuadratic", lightQuadraticFalloff);
		shader->SetUniform("u_NoLight", NoLight);
		shader->SetUniform("u_Ambient", Ambient);
		shader->SetUniform("u_Specular", Specular);
		shader->SetUniform("u_AmbientSpecular", AmbientSpecular);
		shader->SetUniform("u_AmbientSpecularBloom", AmbientSpecularBloom);
		shader->SetUniform("u_Textures", Textures);

		PostEffect* basicEffect;

		int activeEffect = 0;
		std::vector<PostEffect*> effects;

		SepiaEffect* sepiaEffect;
		GreyscaleEffect* greyscaleEffect;
		ColorCorrectEffect* colorCorrectEffect;
		BlurEffect* blurEffect;
		

		// We'll add some ImGui controls to control our shader
		BackendHandler::imGuiCallbacks.push_back([&]() {
			if (ImGui::CollapsingHeader("Effect controls"))
			{
				ImGui::SliderInt("Chosen Effect", &activeEffect, 0, effects.size() - 1);

				if (activeEffect == 0)
				{
					ImGui::Text("Active Effect: Sepia Effect");

					SepiaEffect* temp = (SepiaEffect*)effects[activeEffect];
					float intensity = temp->GetIntensity();

					if (ImGui::SliderFloat("Intensity", &intensity, 0.0f, 1.0f))
					{
						temp->SetIntensity(intensity);
					}
				}
				if (activeEffect == 1)
				{
					ImGui::Text("Active Effect: Greyscale Effect");
					
					GreyscaleEffect* temp = (GreyscaleEffect*)effects[activeEffect];
					float intensity = temp->GetIntensity();

					if (ImGui::SliderFloat("Intensity", &intensity, 0.0f, 1.0f))
					{
						temp->SetIntensity(intensity);
					}
				}
				if (activeEffect == 2)
				{
					ImGui::Text("Active Effect: Color Correct Effect");

					ColorCorrectEffect* temp = (ColorCorrectEffect*)effects[activeEffect];
					static char input[BUFSIZ];
					ImGui::InputText("Lut File to Use", input, BUFSIZ);

					if (ImGui::Button("SetLUT", ImVec2(200.0f, 40.0f)))
					{
						temp->SetLUT(LUT3D(std::string(input)));
					}
				}
				
				if (activeEffect == 3)
				{
					ImGui::Text("Active Effect: Bloom Effect");

					BlurEffect* temp = (BlurEffect*)effects[activeEffect];
					float threshold = temp->GetThreshold();

					if (ImGui::SliderFloat("Threshold", &threshold, 0.0f, 1.0f))
					{
						temp->SetThreshold(threshold);
					}
					
					BlurEffect* tempa = (BlurEffect*)effects[activeEffect];
					float Passes = tempa->GetPasses();

					if (ImGui::SliderFloat("Blur", &Passes, 0.0f, 10.0f))
					{
						tempa->SetPasses(Passes);
					}
				}
			}
			if (ImGui::CollapsingHeader("Environment generation"))
			{
				if (ImGui::Button("Regenerate Environment", ImVec2(200.0f, 40.0f)))
				{
					EnvironmentGenerator::RegenerateEnvironment();
				}
			}
			if (ImGui::CollapsingHeader("Scene Level Lighting Settings"))
			{
				if (ImGui::ColorPicker3("Ambient Color", glm::value_ptr(ambientCol))) {
					shader->SetUniform("u_AmbientCol", ambientCol);
				}
				if (ImGui::SliderFloat("Fixed Ambient Power", &ambientPow, 0.01f, 1.0f)) {
					shader->SetUniform("u_AmbientStrength", ambientPow);
				}
			}
			if (ImGui::CollapsingHeader("Light Level Lighting Settings"))
			{
				if (ImGui::DragFloat3("Light Pos", glm::value_ptr(lightPos), 0.01f, -10.0f, 10.0f)) {
					shader->SetUniform("u_LightPos", lightPos);
				}
				if (ImGui::ColorPicker3("Light Col", glm::value_ptr(lightCol))) {
					shader->SetUniform("u_LightCol", lightCol);
				}
				if (ImGui::SliderFloat("Light Ambient Power", &lightAmbientPow, 0.0f, 1.0f)) {
					shader->SetUniform("u_AmbientLightStrength", lightAmbientPow);
				}
				if (ImGui::SliderFloat("Light Specular Power", &lightSpecularPow, 0.0f, 1.0f)) {
					shader->SetUniform("u_SpecularLightStrength", lightSpecularPow);
				}
				if (ImGui::DragFloat("Light Linear Falloff", &lightLinearFalloff, 0.01f, 0.0f, 1.0f)) {
					shader->SetUniform("u_LightAttenuationLinear", lightLinearFalloff);
				}
				if (ImGui::DragFloat("Light Quadratic Falloff", &lightQuadraticFalloff, 0.01f, 0.0f, 1.0f)) {
					shader->SetUniform("u_LightAttenuationQuadratic", lightQuadraticFalloff);
				}
			}
			if (ImGui::CollapsingHeader("Toggle Buttons"))
			{
				if (ImGui::Button("No Lighting"))
				{
					shader->SetUniform("u_NoLight", NoLight = 1);
					shader->SetUniform("u_Ambient", Ambient = 0);
					shader->SetUniform("u_Specular", Specular = 0);
					shader->SetUniform("u_AmbientSpecular", AmbientSpecular = 0);
					shader->SetUniform("u_AmbientSpecularBloom", AmbientSpecularBloom = 0);
					shader->SetUniform("u_Textures", Textures = 2);
				}
				if (ImGui::Button("Ambient Lighting only"))
				{
					shader->SetUniform("u_NoLight", NoLight = 0);
					shader->SetUniform("u_Ambient", Ambient = 1);
					shader->SetUniform("u_Specular", Specular = 0);
					shader->SetUniform("u_AmbientSpecular", AmbientSpecular = 0);
					shader->SetUniform("u_AmbientSpecularBloom", AmbientSpecularBloom = 0);
					shader->SetUniform("u_Textures", Textures = 2);
				}
				if (ImGui::Button("Specular Lighting only"))
				{
					shader->SetUniform("u_NoLight", NoLight = 0);
					shader->SetUniform("u_Ambient", Ambient = 0);
					shader->SetUniform("u_Specular", Specular = 1);
					shader->SetUniform("u_AmbientSpecular", AmbientSpecular = 0);
					shader->SetUniform("u_AmbientSpecularBloom", AmbientSpecularBloom = 0);
					shader->SetUniform("u_Textures", Textures = 2);
				}
				if (ImGui::Button("Ambient + Specular Lighting"))
				{
					shader->SetUniform("u_NoLight", NoLight = 0);
					shader->SetUniform("u_Ambient", Ambient = 0);
					shader->SetUniform("u_Specular", Specular = 0);
					shader->SetUniform("u_AmbientSpecular", AmbientSpecular = 1);
					shader->SetUniform("u_AmbientSpecularBloom", AmbientSpecularBloom = 0);
					shader->SetUniform("u_Textures", Textures = 2);
				}
				if (ImGui::Button("Ambient + Specular + Diffuse"))
				{
					shader->SetUniform("u_NoLight", NoLight = 0);
					shader->SetUniform("u_Ambient", Ambient = 0);
					shader->SetUniform("u_Specular", Specular = 0);
					shader->SetUniform("u_AmbientSpecular", AmbientSpecular = 0);
					shader->SetUniform("u_AmbientSpecularBloom", AmbientSpecularBloom = 1);
					shader->SetUniform("u_Textures", Textures = 2);
				}
				if (OnOff) {
					if (ImGui::Button("Textures Off"))
					{
						shader->SetUniform("u_Textures", Textures = 0);
						OnOff = false;
					}
				}
				else {
					if (ImGui::Button("Textures On"))
					{
						shader->SetUniform("u_Textures", Textures = 1);
						OnOff = true;
					}
				}
			}

			auto name = controllables[selectedVao].get<GameObjectTag>().Name;
			ImGui::Text(name.c_str());
			auto behaviour = BehaviourBinding::Get<SimpleMoveBehaviour>(controllables[selectedVao]);
			ImGui::Checkbox("Relative Rotation", &behaviour->Relative);

			ImGui::Text("Q/E -> Yaw\nLeft/Right -> Roll\nUp/Down -> Pitch\nY -> Toggle Mode");
		
			minFps = FLT_MAX;
			maxFps = 0;
			avgFps = 0;
			for (int ix = 0; ix < 128; ix++) {
				if (fpsBuffer[ix] < minFps) { minFps = fpsBuffer[ix]; }
				if (fpsBuffer[ix] > maxFps) { maxFps = fpsBuffer[ix]; }
				avgFps += fpsBuffer[ix];
			}
			ImGui::PlotLines("FPS", fpsBuffer, 128);
			ImGui::Text("MIN: %f MAX: %f AVG: %f", minFps, maxFps, avgFps / 128.0f);
			});

		#pragma endregion 

		// GL states
		glEnable(GL_DEPTH_TEST);
		//glEnable(GL_CULL_FACE);
		glDepthFunc(GL_LEQUAL); // New 

		#pragma region TEXTURE LOADING

		// Load some textures from files
		Texture2D::sptr stone = Texture2D::LoadFromFile("images/Stone_001_Diffuse.png");
		Texture2D::sptr stoneSpec = Texture2D::LoadFromFile("images/Stone_001_Specular.png");
		Texture2D::sptr grass = Texture2D::LoadFromFile("images/grass.jpg");
		Texture2D::sptr noSpec = Texture2D::LoadFromFile("images/grassSpec.png");
		Texture2D::sptr box = Texture2D::LoadFromFile("images/box.bmp");
		Texture2D::sptr boxSpec = Texture2D::LoadFromFile("images/box-reflections.bmp");
		Texture2D::sptr simpleFlora = Texture2D::LoadFromFile("images/SimpleFlora.png");
		Texture2D::sptr Table = Texture2D::LoadFromFile("images/Table.png");
		Texture2D::sptr Fence = Texture2D::LoadFromFile("images/Diffuse.jpg");
		Texture2D::sptr White = Texture2D::LoadFromFile("images/White.png");
		Texture2D::sptr ballwhite = Texture2D::LoadFromFile("images/10536_soccerball_V1_diffuse.jpg");
		Texture2D::sptr gazebo = Texture2D::LoadFromFile("images/10073_Gazebo_V2_Diffuse.jpg");
		LUT3D testCube("cubes/BrightenedCorrection.cube");

		// Load the cube map
		//TextureCubeMap::sptr environmentMap = TextureCubeMap::LoadFromImages("images/cubemaps/skybox/sample.jpg");
		TextureCubeMap::sptr environmentMap = TextureCubeMap::LoadFromImages("images/cubemaps/skybox/ToonSky.jpg"); 

		// Creating an empty texture
		Texture2DDescription desc = Texture2DDescription();  
		desc.Width = 1;
		desc.Height = 1;
		desc.Format = InternalFormat::RGB8;
		Texture2D::sptr texture2 = Texture2D::Create(desc);
		// Clear it with a white colour
		texture2->Clear();

		#pragma endregion

		///////////////////////////////////// Scene Generation //////////////////////////////////////////////////
		#pragma region Scene Generation
		
		// We need to tell our scene system what extra component types we want to support
		GameScene::RegisterComponentType<RendererComponent>();
		GameScene::RegisterComponentType<BehaviourBinding>();
		GameScene::RegisterComponentType<Camera>();

		// Create a scene, and set it to be the active scene in the application
		GameScene::sptr scene = GameScene::Create("test");
		Application::Instance().ActiveScene = scene;

		// We can create a group ahead of time to make iterating on the group faster
		entt::basic_group<entt::entity, entt::exclude_t<>, entt::get_t<Transform>, RendererComponent> renderGroup =
			scene->Registry().group<RendererComponent>(entt::get_t<Transform>());

		// Create a material and set some properties for it
		ShaderMaterial::sptr GrassMat = ShaderMaterial::Create();  
		GrassMat->Shader = shader;
		GrassMat->Set("s_Diffuse", grass);
		GrassMat->Set("s_Specular", stoneSpec);
		GrassMat->Set("u_Shininess", 2.0f);
		GrassMat->Set("u_TextureMix", 0.0f);
		
		ShaderMaterial::sptr FenceMat = ShaderMaterial::Create();  
		FenceMat->Shader = shader;
		FenceMat->Set("s_Diffuse", Fence);
		FenceMat->Set("s_Specular", stoneSpec);
		FenceMat->Set("u_Shininess", 2.0f);
		FenceMat->Set("u_TextureMix", 0.0f); 

		ShaderMaterial::sptr WhiteMat = ShaderMaterial::Create();
		WhiteMat->Shader = shader;
		WhiteMat->Set("s_Diffuse", White);
		WhiteMat->Set("s_Specular", boxSpec);
		WhiteMat->Set("u_Shininess", 8.0f);
		WhiteMat->Set("u_TextureMix", 0.0f);

		ShaderMaterial::sptr ballMat = ShaderMaterial::Create();
		ballMat->Shader = shader;
		ballMat->Set("s_Diffuse", ballwhite);
		ballMat->Set("s_Specular", noSpec);
		ballMat->Set("u_Shininess", 8.0f);
		ballMat->Set("u_TextureMix", 0.5f);
		
		ShaderMaterial::sptr TableMat = ShaderMaterial::Create();
		TableMat->Shader = shader;
		TableMat->Set("s_Diffuse", Table);
		TableMat->Set("s_Specular", noSpec);
		TableMat->Set("u_Shininess", 8.0f);
		TableMat->Set("u_TextureMix", 0.0f);
		
		ShaderMaterial::sptr GazeboMat = ShaderMaterial::Create();
		GazeboMat->Shader = shader;
		GazeboMat->Set("s_Diffuse", gazebo);
		GazeboMat->Set("s_Specular", noSpec);
		GazeboMat->Set("u_Shininess", 8.0f);
		GazeboMat->Set("u_TextureMix", 0.0f);

		GameObject obj1 = scene->CreateEntity("Table");
		{
			VertexArrayObject::sptr vao = ObjLoader::LoadFromFile("models/Table.obj");
			obj1.emplace<RendererComponent>().SetMesh(vao).SetMaterial(TableMat);
			obj1.get<Transform>().SetLocalPosition(0.0f, 0.0f, 0.0f);
			obj1.get<Transform>().SetLocalRotation(90.0f, 0.0f, 0.0f);
			obj1.get<Transform>().SetLocalScale(1.0f, 1.0f, 1.0f);
			BehaviourBinding::BindDisabled<SimpleMoveBehaviour>(obj1);
		}

		GameObject obj2 = scene->CreateEntity("Plane");
		{
			VertexArrayObject::sptr vao = ObjLoader::LoadFromFile("models/plane.obj");
			obj2.emplace<RendererComponent>().SetMesh(vao).SetMaterial(GrassMat);
			obj2.get<Transform>().SetLocalPosition(0.0f, 0.0f, 0.0f);
			obj2.get<Transform>().SetLocalRotation(00.0f, 0.0f, -90.0f);
			obj2.get<Transform>().SetLocalScale(1.0f, 1.0f, 1.0f);
			BehaviourBinding::BindDisabled<SimpleMoveBehaviour>(obj2);
		}
		
		std::vector<GameObject> fenceBack;
		{
			VertexArrayObject::sptr vao = ObjLoader::LoadFromFile("models/fence.obj");
			for (int i = 0; i < 5; i++)
			{
				fenceBack.push_back(scene->CreateEntity("fence back" + (std::to_string(i + 1))));
				fenceBack[i].emplace<RendererComponent>().SetMesh(vao).SetMaterial(FenceMat);
				fenceBack[i].get<Transform>().SetLocalPosition(-16.0f + i * 8, 20.0f, 2.0f);
				fenceBack[i].get<Transform>().SetLocalRotation(90.0f, 0.0f, 0.0f);
				fenceBack[i].get<Transform>().SetLocalScale(1.0f, 1.0f, 1.0f);
			}
		}
		
		std::vector<GameObject> fenceRight;
		{
			VertexArrayObject::sptr vao = ObjLoader::LoadFromFile("models/fence.obj");
			for (int i = 0; i < 5; i++)
			{
				fenceRight.push_back(scene->CreateEntity("fence right" + (std::to_string(i + 1))));
				fenceRight[i].emplace<RendererComponent>().SetMesh(vao).SetMaterial(FenceMat);
				fenceRight[i].get<Transform>().SetLocalPosition(20, -16.0f + i * 8, 2.0f);
				fenceRight[i].get<Transform>().SetLocalRotation(90.0f, 0.0f, 90.0f);
				fenceRight[i].get<Transform>().SetLocalScale(1.0f, 1.0f, 1.0f);
			}
		}
		
		std::vector<GameObject> fenceLeft;
		{
			VertexArrayObject::sptr vao = ObjLoader::LoadFromFile("models/fence.obj");
			for (int i = 0; i < 5; i++)
			{
				fenceLeft.push_back(scene->CreateEntity("fence left" + (std::to_string(i + 1))));
				fenceLeft[i].emplace<RendererComponent>().SetMesh(vao).SetMaterial(FenceMat);
				fenceLeft[i].get<Transform>().SetLocalPosition(-20, -15.0f + i * 8, 2.0f);
				fenceLeft[i].get<Transform>().SetLocalRotation(90.0f, 0.0f, 90.0f);
				fenceLeft[i].get<Transform>().SetLocalScale(1.0f, 1.0f, 1.0f);
			}
		}
		
		std::vector<GameObject> fenceUp;
		{
			VertexArrayObject::sptr vao = ObjLoader::LoadFromFile("models/fence.obj");
			for (int i = 0; i < 6; i++)
			{
				fenceUp.push_back(scene->CreateEntity("fence Up" + (std::to_string(i + 1))));
				fenceUp[i].emplace<RendererComponent>().SetMesh(vao).SetMaterial(FenceMat);
				fenceUp[i].get<Transform>().SetLocalPosition(-17.0f + i * 8, -20, 2.0f);
				fenceUp[i].get<Transform>().SetLocalRotation(90.0f, 0.0f, 0.0f);
				fenceUp[i].get<Transform>().SetLocalScale(1.0f, 1.0f, 1.0f);
			}
		}

		GameObject obj7 = scene->CreateEntity("goal");
		{
			VertexArrayObject::sptr vao = ObjLoader::LoadFromFile("models/football goal.obj");
			obj7.emplace<RendererComponent>().SetMesh(vao).SetMaterial(WhiteMat);
			obj7.get<Transform>().SetLocalPosition(12.0f, -15.0f, 0.0f);
			obj7.get<Transform>().SetLocalRotation(90.0f, 0.0f, 90.0f);
			obj7.get<Transform>().SetLocalScale(0.02f, 0.02f, 0.02f);
		}
		
		GameObject obj8 = scene->CreateEntity("goal2");
		{
			VertexArrayObject::sptr vao = ObjLoader::LoadFromFile("models/football goal.obj");
			obj8.emplace<RendererComponent>().SetMesh(vao).SetMaterial(WhiteMat);
			obj8.get<Transform>().SetLocalPosition(-12.0f, -5.0f, 0.0f);
			obj8.get<Transform>().SetLocalRotation(90.0f, 0.0f, -90.0f);
			obj8.get<Transform>().SetLocalScale(0.02f, 0.02f, 0.02f);
		}
		
		GameObject obj9 = scene->CreateEntity("Ball");
		{
			VertexArrayObject::sptr vao = ObjLoader::LoadFromFile("models/ball.obj");
			obj9.emplace<RendererComponent>().SetMesh(vao).SetMaterial(ballMat);
			obj9.get<Transform>().SetLocalPosition(0.0f, -8.0f, 0.0f);
			obj9.get<Transform>().SetLocalRotation(0.0f, 0.0f, 0.0f);
			obj9.get<Transform>().SetLocalScale(0.07f, 0.07f, 0.07f);

			// Bind returns a smart pointer to the behaviour that was added
			auto pathing = BehaviourBinding::Bind<FollowPathBehaviour>(obj9);
			// Set up a path for the object to follow
			pathing->Points.push_back({ -5.0f, -10.0f, 0.0f });
			pathing->Points.push_back({ 5.0f, -10.0f, 0.0f });
			pathing->Points.push_back({ 5.0f, -8.0f, 0.0f });
			pathing->Points.push_back({ -5.0f, -8.0f, 0.0f });
			pathing->Speed = 4.0f;
		}
		
		GameObject obj10 = scene->CreateEntity("Gazebo");
		{
			VertexArrayObject::sptr vao = ObjLoader::LoadFromFile("models/10073_Gazebo_V2_L3.obj");
			obj10.emplace<RendererComponent>().SetMesh(vao).SetMaterial(GazeboMat);
			obj10.get<Transform>().SetLocalPosition(0.0f, 12.0f, 0.0f);
			obj10.get<Transform>().SetLocalRotation(0.0f, 0.0f, 0.0f);
			obj10.get<Transform>().SetLocalScale(0.02f, 0.02f, 0.02f);
		}

		// Create an object to be our camera
		GameObject cameraObject = scene->CreateEntity("Camera");
		{
			cameraObject.get<Transform>().SetLocalPosition(0, 3, 3).LookAt(glm::vec3(0, 0, 0));

			// We'll make our camera a component of the camera object
			Camera& camera = cameraObject.emplace<Camera>();// Camera::Create();
			camera.SetPosition(glm::vec3(0, 3, 3));
			camera.SetUp(glm::vec3(0, 0, 1));
			camera.LookAt(glm::vec3(0));
			camera.SetFovDegrees(90.0f); // Set an initial FOV
			camera.SetOrthoHeight(3.0f);
			BehaviourBinding::Bind<CameraControlBehaviour>(cameraObject);
		}

		int width, height;
		glfwGetWindowSize(BackendHandler::window, &width, &height);

		GameObject framebufferObject = scene->CreateEntity("Basic Effect");
		{
			basicEffect = &framebufferObject.emplace<PostEffect>();
			basicEffect->Init(width, height);
		}

		GameObject sepiaEffectObject = scene->CreateEntity("Sepia Effect");
		{
			sepiaEffect = &sepiaEffectObject.emplace<SepiaEffect>();
			sepiaEffect->Init(width, height);
		}
		effects.push_back(sepiaEffect);

		GameObject greyscaleEffectObject = scene->CreateEntity("Greyscale Effect");
		{
			greyscaleEffect = &greyscaleEffectObject.emplace<GreyscaleEffect>();
			greyscaleEffect->Init(width, height);
		}
		effects.push_back(greyscaleEffect);
		
		GameObject colorCorrectEffectObject = scene->CreateEntity("ColorCorrection Effect");
		{
			colorCorrectEffect = &colorCorrectEffectObject.emplace<ColorCorrectEffect>();
			colorCorrectEffect->Init(width, height);
		}
		effects.push_back(colorCorrectEffect);
		
		GameObject blurEffectObject = scene->CreateEntity("Blur Effect");
		{
			blurEffect = &blurEffectObject.emplace<BlurEffect>();
			blurEffect->Init(width, height);
		}
		effects.push_back(blurEffect);

		#pragma endregion 
		//////////////////////////////////////////////////////////////////////////////////////////

		/////////////////////////////////// SKYBOX ///////////////////////////////////////////////
		{
			// Load our shaders
			Shader::sptr skybox = std::make_shared<Shader>();
			skybox->LoadShaderPartFromFile("shaders/skybox-shader.vert.glsl", GL_VERTEX_SHADER);
			skybox->LoadShaderPartFromFile("shaders/skybox-shader.frag.glsl", GL_FRAGMENT_SHADER);
			skybox->Link();

			ShaderMaterial::sptr skyboxMat = ShaderMaterial::Create();
			skyboxMat->Shader = skybox;  
			skyboxMat->Set("s_Environment", environmentMap);
			skyboxMat->Set("u_EnvironmentRotation", glm::mat3(glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(1, 0, 0))));
			skyboxMat->RenderLayer = 100;

			MeshBuilder<VertexPosNormTexCol> mesh;
			MeshFactory::AddIcoSphere(mesh, glm::vec3(0.0f), 1.0f);
			MeshFactory::InvertFaces(mesh);
			VertexArrayObject::sptr meshVao = mesh.Bake();
			
			GameObject skyboxObj = scene->CreateEntity("skybox");  
			skyboxObj.get<Transform>().SetLocalPosition(0.0f, 0.0f, 0.0f);
			skyboxObj.get_or_emplace<RendererComponent>().SetMesh(meshVao).SetMaterial(skyboxMat);
		}
		////////////////////////////////////////////////////////////////////////////////////////


		// We'll use a vector to store all our key press events for now (this should probably be a behaviour eventually)
		std::vector<KeyPressWatcher> keyToggles;
		{
			// This is an example of a key press handling helper. Look at InputHelpers.h an .cpp to see
			// how this is implemented. Note that the ampersand here is capturing the variables within
			// the scope. If you wanted to do some method on the class, your best bet would be to give it a method and
			// use std::bind
			keyToggles.emplace_back(GLFW_KEY_T, [&]() { cameraObject.get<Camera>().ToggleOrtho(); });

			controllables.push_back(obj2);

			keyToggles.emplace_back(GLFW_KEY_KP_ADD, [&]() {
				BehaviourBinding::Get<SimpleMoveBehaviour>(controllables[selectedVao])->Enabled = false;
				selectedVao++;
				if (selectedVao >= controllables.size())
					selectedVao = 0;
				BehaviourBinding::Get<SimpleMoveBehaviour>(controllables[selectedVao])->Enabled = true;
				});
			keyToggles.emplace_back(GLFW_KEY_KP_SUBTRACT, [&]() {
				BehaviourBinding::Get<SimpleMoveBehaviour>(controllables[selectedVao])->Enabled = false;
				selectedVao--;
				if (selectedVao < 0)
					selectedVao = controllables.size() - 1;
				BehaviourBinding::Get<SimpleMoveBehaviour>(controllables[selectedVao])->Enabled = true;
				});

			keyToggles.emplace_back(GLFW_KEY_Y, [&]() {
				auto behaviour = BehaviourBinding::Get<SimpleMoveBehaviour>(controllables[selectedVao]);
				behaviour->Relative = !behaviour->Relative;
				});
		}

		// Initialize our timing instance and grab a reference for our use
		Timing& time = Timing::Instance();
		time.LastFrame = glfwGetTime();

		///// Game loop /////
		while (!glfwWindowShouldClose(BackendHandler::window)) {
			glfwPollEvents();

			// Update the timing
			time.CurrentFrame = glfwGetTime();
			time.DeltaTime = static_cast<float>(time.CurrentFrame - time.LastFrame);

			time.DeltaTime = time.DeltaTime > 1.0f ? 1.0f : time.DeltaTime;

			// Update our FPS tracker data
			fpsBuffer[frameIx] = 1.0f / time.DeltaTime;
			frameIx++;
			if (frameIx >= 128)
				frameIx = 0;

			// We'll make sure our UI isn't focused before we start handling input for our game
			if (!ImGui::IsAnyWindowFocused()) {
				// We need to poll our key watchers so they can do their logic with the GLFW state
				// Note that since we want to make sure we don't copy our key handlers, we need a const
				// reference!
				for (const KeyPressWatcher& watcher : keyToggles) {
					watcher.Poll(BackendHandler::window);
				}
			}

			// Iterate over all the behaviour binding components
			scene->Registry().view<BehaviourBinding>().each([&](entt::entity entity, BehaviourBinding& binding) {
				// Iterate over all the behaviour scripts attached to the entity, and update them in sequence (if enabled)
				for (const auto& behaviour : binding.Behaviours) {
					if (behaviour->Enabled) {
						behaviour->Update(entt::handle(scene->Registry(), entity));
					}
				}
			});

			// Clear the screen
			basicEffect->Clear();
			/*greyscaleEffect->Clear();
			sepiaEffect->Clear();*/
			for (int i = 0; i < effects.size(); i++)
			{
				effects[i]->Clear();
			}


			glClearColor(0.08f, 0.17f, 0.31f, 1.0f);
			glEnable(GL_DEPTH_TEST);
			glClearDepth(1.0f);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

			// Update all world matrices for this frame
			scene->Registry().view<Transform>().each([](entt::entity entity, Transform& t) {
				t.UpdateWorldMatrix();
			});
			
			// Grab out camera info from the camera object
			Transform& camTransform = cameraObject.get<Transform>();
			glm::mat4 view = glm::inverse(camTransform.LocalTransform());
			glm::mat4 projection = cameraObject.get<Camera>().GetProjection();
			glm::mat4 viewProjection = projection * view;
						
			// Sort the renderers by shader and material, we will go for a minimizing context switches approach here,
			// but you could for instance sort front to back to optimize for fill rate if you have intensive fragment shaders
			renderGroup.sort<RendererComponent>([](const RendererComponent& l, const RendererComponent& r) {
				// Sort by render layer first, higher numbers get drawn last
				if (l.Material->RenderLayer < r.Material->RenderLayer) return true;
				if (l.Material->RenderLayer > r.Material->RenderLayer) return false;

				// Sort by shader pointer next (so materials using the same shader run sequentially where possible)
				if (l.Material->Shader < r.Material->Shader) return true;
				if (l.Material->Shader > r.Material->Shader) return false;

				// Sort by material pointer last (so we can minimize switching between materials)
				if (l.Material < r.Material) return true;
				if (l.Material > r.Material) return false;
				
				return false;
			});

			// Start by assuming no shader or material is applied
			Shader::sptr current = nullptr;
			ShaderMaterial::sptr currentMat = nullptr;

			basicEffect->BindBuffer(0);

			// Iterate over the render group components and draw them
			renderGroup.each( [&](entt::entity e, RendererComponent& renderer, Transform& transform) {
				// If the shader has changed, set up it's uniforms
				if (current != renderer.Material->Shader) {
					current = renderer.Material->Shader;
					current->Bind();
					BackendHandler::SetupShaderForFrame(current, view, projection);
				}
				// If the material has changed, apply it
				if (currentMat != renderer.Material) {
					currentMat = renderer.Material;
					currentMat->Apply();
				}
				// Render the mesh
				BackendHandler::RenderVAO(renderer.Material->Shader, renderer.Mesh, viewProjection, transform);
			});

			basicEffect->UnbindBuffer();

			effects[activeEffect]->ApplyEffect(basicEffect);
			
			effects[activeEffect]->DrawToScreen();
			
			// Draw our ImGui content
			BackendHandler::RenderImGui();

			scene->Poll();
			glfwSwapBuffers(BackendHandler::window);
			time.LastFrame = time.CurrentFrame;
		}

		// Nullify scene so that we can release references
		Application::Instance().ActiveScene = nullptr;
		//Clean up the environment generator so we can release references
		EnvironmentGenerator::CleanUpPointers();
		BackendHandler::ShutdownImGui();
	}	

	// Clean up the toolkit logger so we don't leak memory
	Logger::Uninitialize();
	return 0;
}