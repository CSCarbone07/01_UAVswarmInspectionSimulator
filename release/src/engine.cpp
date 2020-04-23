#include "sim/engine.hpp"


const GLint WIDTH = 800, HEIGHT = 600;
static GLuint VAO, VBO, IBO, shader, uniformXMove;

static bool direction = true;
static float triOffset = 0.0f;
static float triMaxoffset = 0.7f;
static float triIncrement = 0.005f;
static float tR = 3.14159265f / 180.0f; //conversion to radians
static float currentAngle = 0.0f;
static float currentScale = 0.4f;
static bool scaleDirection = true;
static float maxScale = 0.8f;
static float minScale = 0.1f;

//static const char* vShader = std::ifstream in("FileReadExample.cpp");
//std::string contents((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
//static const char* vShader = "version 330";
/*
    static const char* vShader = "                                 \n\
    #version 330                                                   \n\
    layout (location = 0) in vec3 pos;                             \n\
    void main()                                                    \n\
    {                                                              \n\
    gl_Position = vec4(0.4 * pos.x, 0.4 * pos.y, pos.z, 1.0);      \n\
    }                                                              \n\
                                                                   \n\
   ";
*/

 void Engine::init(YAML::Node config) {


   

    // Initilize agents and world
    maxSteps = config["max_steps"].as<unsigned>();
    numOfAgents = config["num_of_agents"].as<unsigned>();
    communicationsRange = config["communications_range"].as<float>();
    size = {config["world"]["x"].as<unsigned>(), config["world"]["y"].as<unsigned>(), config["world"]["z"].as<unsigned>()};
    //knowledgeBasesFile.open(config["knowledgeBasesFile"].as<std::string>());
    movesFile.open(config["movesFile"].as<std::string>(), std::ios_base::app);
    statusFile.open(config["statusFile"].as<std::string>(), std::ios_base::app);
    randomChoice.open(config["randomChoice"].as<std::string>(), std::ios_base::app);
    timing.open(config["timing"].as<std::string>(), std::ios_base::app);
    visitedCells.open(config["visitedCells"].as<std::string>(), std::ios_base::app);


    float linearVelocity =  config["linearVelocity"].as<float>();
    
    // Orca Variables
    tau = config["tau"].as<float>();
	  collisionAvoidance = config["collisionAvoidance"].as<bool>();
    orcaRadius = config["orcaRadius"].as<float>();

    //Weed Variables
    unsigned clusters = config["weeds"]["clusters"].as<unsigned>();
    unsigned maxweeds = config["weeds"]["maxweeds"].as<unsigned>();
    unsigned isolated = config["weeds"]["isolated"].as<unsigned>();

    //Visit Varibles

    repulsion = config["repulsion"].as<float>();
    attraction = config["attraction"].as<float>();

    knowledgeClusterRadius = config["KnowledgeClusterRadius"].as<float>();

    //Inspection Strategies
    inspectionStrategy = config["InspectionStrategy"].as<std::string>();
    if(inspectionStrategy != "ig")
    {inspectionStrategy = "rw"; }
    std::cout << "Inspection Strategy: " << inspectionStrategy << std::endl;

    if(inspectionStrategy == "ig")
    {
        targetSelectionStrategy = config["TargetSelectionStrategy"].as<std::string>();
        if(targetSelectionStrategy != "greedy" && targetSelectionStrategy != "softmax")
        {
        targetSelectionStrategy = "random";
        }
        if(targetSelectionStrategy == "softmax")
        {
        softmaxLambda = config["Softmax_Lambda"].as<float>();
        std::cout << "Lambda for softmax on target selection strategy: " << softmaxLambda << std::endl; 
        }
        std::cout << "Target selection strategy: " << targetSelectionStrategy << std::endl;
        useSocialInfo = config["UseSocialInfo"].as<bool>();
        std::cout << "Include social info: " << useSocialInfo << std::endl;
        useDistanceForIG = config["UseDistanceForIG"].as<bool>();
        std::cout << "Include distance for IG weights: " << useDistanceForIG << std::endl;        

    }

    std::cout << std::endl; 

    displaySimulation = config["Display_Simulation"].as<bool>();
    
    if(displaySimulation)
    {
    mainWindow = Window(1080, 1080);
	  mainWindow.Initialise();
    CreateShaders();

	  camera = Camera(glm::vec3(size.at(0)/2, size.at(1)/2, 75.0f), glm::vec3(0.0f, 1.0f, 0.0f), -90.0f, 0.0f, 20.0f, 0.5f);

	  projection = glm::perspective(glm::radians(FOV), (GLfloat)mainWindow.getBufferWidth() / mainWindow.getBufferHeight(), 0.1f, 1000.0f);
    
    //CreateObjects();
    //CompileShaders();


    
    if (FT_Init_FreeType(&ft))
    std::cout << "ERROR::FREETYPE: Could not init FreeType Library" << std::endl;

    if (FT_New_Face(ft, "../fonts/Arial/Arial.ttf", 0, &face))
      std::cout << "ERROR::FREETYPE: Failed to load font" << std::endl;  
    
    FT_Set_Pixel_Sizes(face, 0, 48); 

    if (FT_Load_Char(face, 'X', FT_LOAD_RENDER))
      std::cout << "ERROR::FREETYTPE: Failed to load Glyph" << std::endl;  
        
    // Disable byte-alignment restriction
      glPixelStorei(GL_UNPACK_ALIGNMENT, 1); 

    for (GLubyte c = 0; c < 128; c++)
    {
      // Load character glyph 
      if (FT_Load_Char(face, c, FT_LOAD_RENDER))
      {
        std::cout << "ERROR::FREETYTPE: Failed to load Glyph" << std::endl;
        continue;
      }
      // Generate texture
      GLuint texture;
      glGenTextures(1, &texture);
      glBindTexture(GL_TEXTURE_2D, texture);
      glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RED,
        face->glyph->bitmap.width,
        face->glyph->bitmap.rows,
        0,
        GL_RED,
        GL_UNSIGNED_BYTE,
        face->glyph->bitmap.buffer
      );
      // Set texture options
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      // Now store character for later use
      Character character = {
        texture, 
        glm::ivec2(face->glyph->bitmap.width, face->glyph->bitmap.rows),
        glm::ivec2(face->glyph->bitmap_left, face->glyph->bitmap_top),
        face->glyph->advance.x
      };
      Characters.insert(std::pair<GLchar, Character>(c, character));
    }
    
    glBindTexture(GL_TEXTURE_2D, 0);
    // Destroy FreeType once we're finished
    FT_Done_Face(face);
    FT_Done_FreeType(ft);


        // Configure VAO/VBO for texture quads
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * 6 * 4, NULL, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), 0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
      
    }



    //print info
    std::cout << "World size: " << size[0] << "x" << size[1] << "x" << size[2] << std::endl;
    std::cout << "Num Of Agents: " << numOfAgents << std::endl;
    std::cout << "Communications Range";
    if(communicationsRange == -1)
    {
        std::cout << "Communications Range: " << "Unlimited";
    }
    else
    {
        std::cout << "Communications Range: " << communicationsRange;
    }  
    std::cout << std::endl << std::endl;

    std::cout << "Orca Variables:" << std::endl;
    std::cout << "---Tau:" << tau << std::endl;
    std::cout << "---collisionAvoidance enabled:" << collisionAvoidance << std::endl;
    std::cout << "---Agent radius:" << (orcaRadius/2)-0.05f << std::endl;
    std::cout << "---Orca radius:" << orcaRadius << std::endl;
    std::cout << "---Linear Velocity:" << linearVelocity << std::endl << std::endl;

    std::cout << "---Repulsion:" << repulsion << std::endl;
    std::cout << "---Attraction:" << attraction << std::endl << std::endl;

    // initialize the world
    world = new World(size);


    // tree->setParameters(config["delta"].as<float>(),config["xi"].as<float>());
    
    //---------- populate world
    std::cout << "populate world..." << std::endl;

    world->populateAndInitialize(clusters, maxweeds, isolated);


    // initiailize the agents
    agents.reserve(numOfAgents);
    unsigned x,y,z;
    for(unsigned id = 0; id < numOfAgents; ++id) {
      x = RandomGenerator::getInstance().nextInt(size.at(0));
      y = RandomGenerator::getInstance().nextInt(size.at(1));
      z = size.at(2)-1;//zPos.at(RandomGenerator::getInstance().nextInt(0,2));
      while(!world->isPositionFree(x,y,z)){
        x = RandomGenerator::getInstance().nextInt(size.at(0));
        y = RandomGenerator::getInstance().nextInt(size.at(1));
        //z = zPos.at(RandomGenerator::getInstance().nextInt(0,2));
      }
      Agent* agent = new Agent(id, x+0.5, y+0.5, z); // MARK --> change this according to your needs
      if(this->world->addAgent(agent, x+0.5, y+0.5, z)){
        printf("Agent %i in position %f %f %f \n", agent->getId(), agent->getX(), agent->getY(), agent->getZ());
        agent->setVelocity(linearVelocity);
        agent->setAgentRadius(((orcaRadius/2)-0.05f));
        agents.push_back(agent);
      }
    }
   
    //knowledgeBasesFile << "Agents Knowledge Base:" << std::endl;


  }



/**
* Run the simulation.
*/
void Engine::run() {
          
    #ifdef PERFECT_COMMUNICATION

    #endif    

    std::stringstream ss;
    std::cout << "running sim" << std::endl;
    this->world->getPopulationInfo(ss);
    //std::cout << "test" << std::endl;
    movesFile << "population:\n" << ss.rdbuf();
    unsigned timeStep = 0;

    // support vector for shuffled agents
    std::vector<Steppable*> shuffled(agents.begin(), agents.end());
    movesFile << "steps:\n";
    movesFile << ' ' << timeStep << ':' << '\n';
    //add first position
    for(auto a : agents) {
      std::stringstream ss;
      if (a->getInfo(ss))
        movesFile << ' ' << ' ' << ss.rdbuf();
    }
     ++timeStep;
    // do one step until max steps or for infinite time (need to terminate manually)
    unsigned timeToCoverage = 10000;
    unsigned timeToMapping = 10000;
    unsigned timeToMappingOnlyClusters = 10000;
    bool isCovered = false;
    bool isMapped = false;
    bool isMappedOnlyClusters = false;


    while(timeStep < maxSteps || maxSteps == 0) {
    //std::cout << "Time step: " << timeStep << std::endl;

      if(displaySimulation)
      {
      GLfloat now = glfwGetTime(); // SDL_GetPerformanceCounter();
      deltaTime = now - lastTime; // (now - lastTime)*1000/SDL_GetPerformanceFrequency();
      lastTime = now;
      RenderScene();
      }

      if(!isCovered && this->world->isCovered()){
        timeToCoverage = timeStep;
        isCovered = true;
      }
      if(!isMapped && this->world->isMapped()){
        timeToMapping = timeStep;
        isMapped = true;
      }
      if(!isMappedOnlyClusters && this->world->isMappedOnlyClusters()){
        timeToMappingOnlyClusters = timeStep;
        isMappedOnlyClusters = true;                
      }
      if((this->world->isCovered() && this->world->isMapped()) || stop){// || this->world->getTree()->getRoot()->getUtility() == 0.f){
        statusFile << ' '<< timeToCoverage <<' '<<timeToMapping<<' '<<timeToMappingOnlyClusters<<"\n";
        std::cout << "\n\n### world mapped ---- at Step:" << timeStep<<std::endl;
        break;
      }

      // limit the cout
      if(timeStep % 1000 == 0)
        std::cout << "#### time_step: " << timeStep << std::endl;

      // shuffle the agents at every iteration
      std::shuffle(shuffled.begin(), shuffled.end(), RandomGenerator::getInstance().getGenerator());
      movesFile << ' ' << timeStep << ':' << '\n';
      // do step for each agent
      for(auto a : shuffled) {
        a->doStep(timeStep);
        
        std::stringstream ss;
        if (a->getInfo(ss))
          movesFile << ' ' << ' ' << ss.rdbuf();
      }

      randomChoice << ' '<< timeStep <<':'<<' ';
      timing << ' '<< timeStep <<':'<<' ';
      //mappingTime << ' '<< timeStep <<':'<<' ';

      unsigned rand2 = 0;
      for(auto a : agents) {
        if(a->sceltaRandom == true){
          rand++;
          rand2++;
          a->sceltaRandom = false;
        }
      }
      randomChoice << rand<<' '<<rand2<<"\n";
      timing << this->world->remainingTasksToVisit.size()<<' '<<this->world->remainingTasksToMap.size()<<' '<<this->world->remainingTasksIntoClusters.size()<<"\n";
      

      ++timeStep;
       
    }
    
    std::cout<<"celle visitate  =  "<<this->world->getCells().size()-this->world->remainingTasksToVisit.size()<<std::endl;
    if(timeStep == maxSteps){
      statusFile << ' '<< timeToCoverage <<' '<<timeToMapping<<' '<<timeToMappingOnlyClusters<<"\n";
      for(std::map<unsigned, Cell*>::iterator it=this->world->remainingTasksToVisit.begin(); it!=this->world->remainingTasksToVisit.end(); ++it)
        std::cout<<"la cella  "<<it->second->getId()<<"  non è stata visitata"<<std::endl;
      for(std::map<unsigned, Cell*>::iterator it=this->world->remainingTasksToMap.begin(); it!=this->world->remainingTasksToMap.end(); ++it)
        std::cout<<"la cella  "<<it->second->getId()<<"  non è stata mappata"<<std::endl;
    }
    for(Cell * cc : this->world->getCells()){
      std::cout<<"la cella  "<<cc->getId()<<"  con palline  "<<cc->getUtility()<<"  è stata visitata  "<<cc->numOfVisits<<"  volte!"<<std::endl;
      visitedCells<<' '<<cc->getId()<<':'<<' '<<cc->numOfVisits<<"\n";
    }
    unsigned tot_collisions = 0;
    for(Agent* a : agents){
      tot_collisions += a->getCollisions();
      std::cout << "Agent " << a->getId() << " : " << a->getCollisions()  << std::endl;
    } 
    std::cout << "Collisions " << tot_collisions  << std::endl;
}


void Engine::RenderScene()
{
    // Get + handle user input events
    glfwPollEvents();

	camera.keyControl(mainWindow.getKeys(), deltaTime);
	camera.mouseControl(mainWindow.getXChange(), mainWindow.getYChange());

    // clear window
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    //glClear(GL_COLOR_BUFFER_BIT);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    //glUseProgram(shaderList[0].GetShaderID());
	shaderList[0]->UseShader();
	uniformModel = shaderList[0]->GetModelLocation();
	uniformProjection = shaderList[0]->GetProjectionLocation();
	uniformView = shaderList[0]->GetViewLocation();
  uniformInColor = shaderList[0]->GetInColor();

    glUniform1f(shaderList[0]->GetXmoveLocation(), triOffset);
    //glUseProgram(shader1->GetShaderID());
    //glUniform1f(shader1->GetXmoveLocation(), triOffset);
    //glUniform1f(0, triOffset);
    
      
    glm::mat4 model = glm::mat4(1.0f); 
  
  for(Mesh* m : meshList)
  {
  glm::vec4 agentColor = glm::vec4(0.0f, 1.0f, 0.0f, 1.0f);
  model = glm::mat4(1.0f);
	model = glm::translate(model, glm::vec3(m->GetWorldLocation()[0], m->GetWorldLocation()[1], m->GetWorldLocation()[2]));
	model = glm::scale(model, glm::vec3(m->GetWorldScale()[0], m->GetWorldScale()[1], m->GetWorldScale()[2]));
  glUniform4fv(uniformInColor, 1, glm::value_ptr(m->getCurrentColor())); 
	glUniformMatrix4fv(uniformModel, 1, GL_FALSE, glm::value_ptr(model));
	glUniformMatrix4fv(uniformProjection, 1, GL_FALSE, glm::value_ptr(projection));
	glUniformMatrix4fv(uniformView, 1, GL_FALSE, glm::value_ptr(camera.calculateViewMatrix()));
	if(m != nullptr)
  {m->RenderMesh();}
  }

  //RenderText("This is sample text", 25.0f, 25.0f, 1.0f, glm::vec3(0.5, 0.8f, 0.2f));
  //RenderText("(C) LearnOpenGL.com", 540.0f, 570.0f, 0.5f, glm::vec3(0.3, 0.7f, 0.9f));

    glUseProgram(0);

    mainWindow.swapBuffers();


}



void Engine::CreateShaders()
{
	shader1 = new Shader(0);
	shader1->CreateFromFiles(vShader, fShader);
	shaderList.push_back(shader1);

	shader2 = new Shader(1);
	shader2->CreateFromFiles(vTextShader, fTextShader);
	shaderList.push_back(shader1);

    printf("Shaders created \n");
}


void Engine::AddMesh(Mesh* inMesh)
{
    meshList.push_back(inMesh);
}


void Engine::RenderText(std::string text, GLfloat x, GLfloat y, GLfloat scale, glm::vec3 color)
{
    glm::vec4 textColor = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);

    // Activate corresponding render state	
    shader2->UseShader();
    uniformModel = shader2->GetModelLocation();
    uniformProjection = shader2->GetProjectionLocation();
    uniformView = shader2->GetViewLocation();
    uniformInColor = shader2->GetInColor();

    glUniform4fv(uniformInColor, 1, glm::value_ptr(textColor)); 
    //glUniform3f(glGetUniformLocation(shader.get, "textColor"), color.x, color.y, color.z);
    glActiveTexture(GL_TEXTURE0);
    glBindVertexArray(VAO);

    // Iterate through all characters
    std::string::const_iterator c;
    for (c = text.begin(); c != text.end(); c++) 
    {
        Character ch = Characters[*c];

        GLfloat xpos = x + ch.Bearing.x * scale;
        GLfloat ypos = y - (ch.Size.y - ch.Bearing.y) * scale;

        GLfloat w = ch.Size.x * scale;
        GLfloat h = ch.Size.y * scale;
        // Update VBO for each character
        GLfloat vertices[6][4] = {
            { xpos,     ypos + h,   0.0, 0.0 },            
            { xpos,     ypos,       0.0, 1.0 },
            { xpos + w, ypos,       1.0, 1.0 },

            { xpos,     ypos + h,   0.0, 0.0 },
            { xpos + w, ypos,       1.0, 1.0 },
            { xpos + w, ypos + h,   1.0, 0.0 }           
        };
        // Render glyph texture over quad
        glBindTexture(GL_TEXTURE_2D, ch.TextureID);
        // Update content of VBO memory
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices); // Be sure to use glBufferSubData and not glBufferData

        glBindBuffer(GL_ARRAY_BUFFER, 0);
        // Render quad
        glDrawArrays(GL_TRIANGLES, 0, 6);
        // Now advance cursors for next glyph (note that advance is number of 1/64 pixels)
        x += (ch.Advance >> 6) * scale; // Bitshift by 6 to get value in pixels (2^6 = 64 (divide amount of 1/64th pixels by 64 to get amount of pixels))
    }
    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
}



