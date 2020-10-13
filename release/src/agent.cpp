#include "agent/agent.hpp"
#include "util/randomgenerator.hpp"
#include "sim/engine.hpp"
#include "eigen3/Eigen/Dense"
#include "collisionavoidance/orca.hpp"
#include "movementstrategies/randomwalk.hpp"
#include "movementstrategies/informationGain.hpp"

#include <boost/math/distributions/poisson.hpp>


//#define ONLY_COVERAGE
//#define IG
//#define PERFECT_COMMUNICATION      //according to the same define in informationGain.cpp or randomwalk.cpp


Agent::Agent(unsigned id, float x, float y, float z) {
  if(Engine::getInstance().getDisplaySimulation())
  { 
    mesh = new Mesh();
    mesh->SetParent(this);
    mesh->SetInvertedPyramid();
    ChangeColor(glm::vec4(0.0f,1.0f,0.0f,1.0f));
  }


  this->id = id;
  this->position = {float(x),float(y),float(z)};
  this->target = {-1,-1,-1};
  this->targetId = -2;
  this->velocity = {0,0,0};
  this->timeStep = 0;

  SetWorldLocation(std::vector<float> {float(x),float(y),float(z)});
  SetWorldScale(std::vector<float> {1.0f, 1.0f, 1.0f});

  this->communicationsRange = Engine::getInstance().getCommunicationsRange();  
  if(communicationsRange > 0)
  {
  knowledgeBaseLocation = "local";     
  }
  if(communicationsRange == -1)
  {
  knowledgeBaseLocation = "world";
  }

  this->limitForTargetReselection = Engine::getInstance().getlimitForTargetReselection();  

  this->knowledgeClusterRadius = Engine::getInstance().getKnowledgeClusterRadius(); 
  this->currentInspectionStrategy = Engine::getInstance().getInspectionStrategy();  
  this->targetSelectionStrategy = Engine::getInstance().getTargetSelectionStrategy();
  this->softmaxLambda = Engine::getInstance().getSoftmaxLambda();
  this->useSocialInfo = Engine::getInstance().getUseSocialInfo(); 
  this->useDistanceForIG = Engine::getInstance().getUseDistanceForIG(); 
  this->useCommsForSocialIG = Engine::getInstance().getUseCommsForSocialIG(); 
  
  bool DEBUG_THIS = false;
  if(DEBUG_THIS && id == testingId)
  {
    std::cout << "Agent " << id << " considering distance for IG " << useDistanceForIG << std::endl;
  }

  std::array<unsigned,3> size = Engine::getInstance().getWorld()->getSize();

  unsigned cellId = 0;
  unsigned cellSize = 1;
  for(unsigned y=0; y<size.at(1); y++){
    for(unsigned x=0; x<size.at(0); x++){
      Cell* spawnedCell = new Cell(cellId,x,y,0,cellSize, 0);
      spawnedCell->setUtility(Engine::getInstance().getWorld()->getCells().at(cellId)->getUtility());
      this->cells.insert(std::make_pair<>(cellId, spawnedCell));
      this->cellsPointers.push_back(spawnedCell);
      cellId++;
    }
  }
  if(communicationsRange != -1)
  {
    for (Cell* c : this->cellsPointers)
    {c->SetNeighbors(this->cellsPointers);}
  }

    if(currentInspectionStrategy == "ig")
    {ig = new InformationGainStrategy(this);}
    else
    {rw = new RandomWalkStrategy(this);}

}

Agent::Action Agent::nextAction()
{
  bool DEBUG_THIS = false;

  if(this->getTargetX() != -1)
  {
    if(checkTargetReached()) 
    {
      if(this->getTargetZ() == 0)
      {
        if(DEBUG_THIS  && (id == testingId || id == testingId_2))
        {printf("SCAN\n");}
        return SCAN;
      }
      else
        if(DEBUG_THIS  && (id == testingId || id == testingId_2))
        {printf("NONE\n");}
        return NONE;
        //return MOVE;
    }
    else
    {
      if(DEBUG_THIS  && (id == testingId || id == testingId_2))
      {printf("MOVE\n");}
      return MOVE;
    }
  }
  else
  {
    if(DEBUG_THIS  && (id == testingId || id == testingId_2))
    {printf("PICK\n");}
    return PICK;
  }
}

void Agent::forgetTarget()
{
  targetTravelTime = 0;
  Cell* cellToForget;
  if(this->targetId != -1)
  {

    if(communicationsRange == -1)
    {
        cellToForget = Engine::getInstance().getWorld()->getCells().at(this->getTargetId());
    }
    if(communicationsRange>0)
    {
        cellToForget = this->cells.at(this->getTargetId());
    }
    cellToForget->isTargetOf.clear();
    BroadcastCell(this, cellToForget);

    this->target = {-1,-1,-1};
    this->targetId = -1;
  }
  


}

std::array<float,3> Agent::getNextPosition(){
  // Variables for storing optimal velocity
  float optimalVx = 0.0;
  float optimalVy = 0.0;
  float optimalVz = 0.0;
  // Move

  float newX = this->getX();
  float newY = this->getY();
  float newZ = this->getZ();
  std::vector<Agent*> others;
  // Difference between target and my position
  float diff_x = this->getTargetX() - this->getX();
  float diff_y = this->getTargetY() - this->getY();
  float diff_z = this->getTargetZ() - this->getZ();

  // Angle to target
  float alpha = atan2(diff_y,diff_x);
  
  // float beta = atan2(diff_z,diff_x);

// //Compute optimal velocity
	if(diff_y < 0)
	    optimalVy = -float(fmin(std::abs(diff_y), this->getLinearVelocity()));
	else
	    optimalVy = float(fmin(std::abs(diff_y), this->getLinearVelocity()));

	if(diff_x < 0)
	    optimalVx = -float(fmin(std::abs(diff_x), this->getLinearVelocity()));
	else
	    optimalVx = float(fmin(std::abs(diff_x), this->getLinearVelocity()));
  
  if(diff_z < 0)
	    optimalVz = -float(fmin(std::abs(diff_z),this->getLinearVelocity()));
	else
	    optimalVz = float(fmin(std::abs(diff_z), this->getLinearVelocity()));

  // optimalVx = cos(alpha) * this->getLinearVelocity();
  // optimalVy = sin(alpha) * this->getLinearVelocity();
  // optimalVz = sin(beta) * this->getLinearVelocity();
  // optimalVx = float(fmin(diff_x, this->getLinearVelocity()));
  // optimalVy = float(fmin(diff_y, this->getLinearVelocity()));
  // optimalVz = float(fmin(diff_z, this->getLinearVelocity()));

  // Orca
  float dt = 1;
  // Ideal orca velocity
  std::array<float,2> orca_velocity;
  orca_velocity={optimalVx, optimalVy};
  
  // float distance = calculateLinearDistanceToTarget();
  // if(distance < sqrt(optimalVx*optimalVx + optimalVy*optimalVy + optimalVz*optimalVz) || distance <= 0.1){
  //   orca_velocity = { diff_x, diff_y};
  //   optimalVx = diff_x;
  //   optimalVy = diff_y;
  //   optimalVz = diff_z;
  // }

  if(this->getTargetX() > -1){
    // If Collision Avoidance is enabled
    if(Engine::getInstance().getCollisionAvoidance()){
      if(Engine::getInstance().getWorld()->getAgents().size() > 1){
            Eigen::Vector2f orca_vel;
            orca_vel << orca_velocity.at(0), orca_velocity.at(1);
            Eigen::Vector2f new_vel = Orca::compute_orca(this, orca_vel, Engine::getInstance().getWorld()->committedAgents, Engine::getInstance().getTau(), dt);
            optimalVx = new_vel(0);
            optimalVy = new_vel(1);
            //assert(optimalVx==0);
      }
    }
  }
  this->velocity.at(0) = optimalVx;
  this->velocity.at(1) = optimalVy;
  this->velocity.at(2) = optimalVz;
  this->theta = alpha;
  std::array<float,3> nextPose;
  nextPose = { newX + optimalVx, newY + optimalVy, newZ+optimalVz };
  if(nextPose.at(2) < 0.5)
    this->checkCollision(nextPose,Engine::getInstance().getWorld()->getAgents());
  return nextPose;
}

bool Agent::checkCollision(std::array<float,3> nextPose,std::vector<Agent*> others){
  for(Agent* a : others){
    if(a->getId() == this->getId())
      continue;
    if(sqrt(pow((nextPose.at(0)-a->getX()),2)+pow((nextPose.at(1) - a->getY()),2)+pow((nextPose.at(2)-a->getZ()),2)) < (2*this->agentRadius)){
      this->collisions++;
      return true;
    }
  }
  return false;
}

void Agent::getNextTarget(){
  
  std::array<float,3> newTarget;

//choose the strategy IG or RW
if(currentInspectionStrategy == "ig")
{newTarget = ig->pickNextTarget(this);}
else
{newTarget = rw->pickNextTarget(this);}


        //debug lines (can delete)       
        //int tempTargetID=Engine::getInstance().getWorld()->getCellId(newTarget.at(0), newTarget.at(1), newTarget.at(2));   
        //std::cout << "Debug ID cell " << tempTargetID << " has " << cells.at(tempTargetID)->isTargetOf.size() << " agents that had it as target." << std::endl;
        //end of debug lines 

  if(newTarget.at(0) == -1)
  {
    //the agent has no task! increase in altitude so as not to interfere with ORCA
    newTarget = {this->getX(), this->getY(), 4};
  }

  this->target = {float(newTarget.at(0)),float(newTarget.at(1)),float(newTarget.at(2))};



}

           
void Agent::BroadcastCell(Agent* agent, Cell* cellToSend)
{

    bool DEBUG_FUNCTION = false;

    if(DEBUG_FUNCTION && id == testingId && agent == this)
    {
      std::cout << std::endl << "Code agent " << id << " starting broadcast" << std::endl;
    }

    ChangeColor(sendingMessageColor);
     
    //std::cout << "Agent: " << this->getId() << ". Broadcasting through agent " << agent->getId() <<  " cell: " << cellToSend->getId() << " with " 
    //<< cellToSend->observationVectors.size()  << " observations. Mapping: " << cellToSend->isMapped() << std::endl;
       

    if(communicationsRange == -1)
    {
    Cell* worldCell_REF = Engine::getInstance().getWorld()->getCells().at(cellToSend->getId());
    worldCell_REF->isTargetOf = cellToSend->isTargetOf;
    worldCell_REF->observationVector = cellToSend->observationVector;
    worldCell_REF->knowledgeVector = cellToSend->knowledgeVector;
    //std::cout << "test" << std::endl;  

    //worldCell_REF->observationVectors.insert(cellToSend->observationVectors.begin(), cellToSend->observationVectors.end());
    //worldCell_REF->knowledgeVectors.insert(cellToSend->knowledgeVectors.begin(), cellToSend->knowledgeVectors.end());

  /*
    std::cout << "World knowledge base recieving cell " << worldCell_REF->getId()<< " observations and isTargetOf agent "; 
    if(cellToSend->isTargetOf.size() >= 1)
    {std::cout << cellToSend->isTargetOf.at(0) << std::endl;}
    else
    {std::cout << "NONE" << std::endl;}
  */ 

    }
    if(communicationsRange > 0)
    {
      
      for(auto t : Engine::getInstance().getWorld()->getAgents())
      {
        if(agent->getId() != t->getId() && std::find(agentsBroadcasting.begin(), agentsBroadcasting.end(), t) == agentsBroadcasting.end())
        { 
          float distance_t = t->calculateLinearDistanceToTarget(agent->getPosition());
          if( distance_t != 0 && distance_t <= agent->communicationsRange)
          {
            if(DEBUG_FUNCTION && id == testingId || id == testingId_2)
            {
            std::cout << "Code agent " << id << " in time step " << timeStep << "." << " Agent " << agent->getId() << " at " << agent->getX() << "x + " << agent->getY() << "y + " << agent->getZ() << "z"
            <<" sending cell " << cellToSend->getId() << " to agent " << t->getId() << " within " << distance_t << "m of distance" << std::endl;
            }
            
            t->ReceiveCell(this, cellToSend); 
            if(rebroadcast)
            {
              agentsBroadcasting.push_back(agent);
              BroadcastCell(t, cellToSend);
            }
            //if(agent == this)
            //{agentsBroadcasting.clear();} 
                        
          }   
        }
      } 
        
    if(agent == this && rebroadcast)
    {agentsBroadcasting.clear();} 

    }
    //std::cout << "test" << std::endl;  

}


void Agent::ReceiveCell(Agent* sendingAgent, Cell* receivedCell) //recieving broadcasted latest observation of cell by agent to updating knowledge 
{
    bool DEBUG_FUNCTION = false;

    if(mesh != nullptr)
    {ChangeColor(receivingMessageColor);}
//    Engine::getInstance()

    if(DEBUG_FUNCTION && (sendingAgent->getId() == testingId || sendingAgent->getId() == testingId_2))
    {
    std::cout << "Code agent " << id << " in time step " << timeStep << "." << " Agent " << this->getId() << " at " << this->getX() << "x + " << this->getY() << "y + " << this->getZ() << "z"
    << " Recieving cell: " << receivedCell->getId() << " from agent " << sendingAgent->getId() << ". Its current target cell is: " << targetId << std::endl;
    }



    Cell* updatingCell = this->cells.at(receivedCell->getId());
    updatingCell->isTargetOf = receivedCell->isTargetOf;
    
    if(!(updatingCell->isMapped())) // checking if the cell is mapped or not in the current agent KB
    {
      if(receivedCell->isMapped())
      {
        if(DEBUG_FUNCTION && (sendingAgent->getId() == testingId || sendingAgent->getId() == testingId_2))
        {
          std::cout << "KB updated: it was mapped" << std::endl;
        }

        updatingCell->setMapped();
        if(currentInspectionStrategy == "rw")
        {
        updatingCell->setBeacon(receivedCell->getBeacon());
        this->beacons.insert(std::make_pair<>(receivedCell->getId(), this->cells.at(receivedCell->getId())));
        this->beacons.at(receivedCell->getId()) = this->cells.at(receivedCell->getId());
        }
        /*
        updatingCell->observationVector = receivedCell->observationVector;
        updatingCell->knowledgeVector = receivedCell->knowledgeVector;
        updatingCell->lastTimeVisit = receivedCell->lastTimeVisit;
        */
      }
      else
      {
        if(receivedCell->getLastWeeedsSeen() >= 0)
        {

          if(DEBUG_FUNCTION && (sendingAgent->getId() == testingId || sendingAgent->getId() == testingId_2))
          {
            std::cout << "KB updated for weeds seen " << receivedCell->getLastWeeedsSeen() << std::endl;
          }

          scanCurrentLocation(updatingCell, receivedCell->getLastWeeedsSeen());
          if(currentInspectionStrategy == "rw")
          {
          updatingCell->setBeacon(receivedCell->getBeacon());
          this->beacons.insert(std::make_pair<>(receivedCell->getId(), this->cells.at(receivedCell->getId())));
          this->beacons.at(receivedCell->getId()) = this->cells.at(receivedCell->getId());
          }
        }
        
        /*
        if(receivedCell->lastTimeVisit > updatingCell->lastTimeVisit)
        {

          if(DEBUG_FUNCTION && (sendingAgent->getId() == testingId || sendingAgent->getId() == testingId_2))
          {
            std::cout << "KB updated for time step " << receivedCell->lastTimeVisit 
            << ". It was not mapped and last visit was at time step " << updatingCell->lastTimeVisit << std::endl;
          }

          
          updatingCell->observationVector = receivedCell->observationVector;
          updatingCell->knowledgeVector = receivedCell->knowledgeVector;
          updatingCell->lastTimeVisit = receivedCell->lastTimeVisit;
          
          updatingCell->setBeacon(receivedCell->getBeacon());
          this->beacons.insert(std::make_pair<>(receivedCell->getId(), this->cells.at(receivedCell->getId())));
          this->beacons.at(receivedCell->getId()) = this->cells.at(receivedCell->getId());
        }
        else
        {
          if(DEBUG_FUNCTION && (sendingAgent->getId() == testingId || sendingAgent->getId() == testingId_2))
          {
            std::cout << "KB not updated, last visit was at " << receivedCell->lastTimeVisit << " and transmission is for last visit at " 
            << updatingCell->lastTimeVisit << std::endl;
          }
        }
        */

      }
    }
    else
    {
      if(DEBUG_FUNCTION && (sendingAgent->getId() == testingId || sendingAgent->getId() == testingId_2))
      {
        std::cout << "cell was already mapped" << std::endl;
      }

    }
    
}


bool Agent::doStep(unsigned timeStep){

  this->timeStep = timeStep;  

  SetWorldLocation(std::vector<float> {getX(), getY(), getZ()});
  //mesh->SetWorldLocation(std::vector<float> {getX(), getY(), getZ()});

  switch(nextAction()){
    case PICK:
    {
        bool DEBUG_THIS = false;
        //std::cout << "Agent " << this->getId() << " currently at: " << this->getX() << "x + " << this->getY() << "y + " << this->getZ() << "z"
        //<< " is picking its target cell using: " << this->currentInspectionStrategy << " strategy" << std::endl;


        this->velocity = {0.0,0.0,0.0};
        getNextTarget();

        if(DEBUG_THIS  && (id == testingId || id == testingId_2))
        {
          std::cout << "Target picked at " << this->getTargetX() << "x, " << this->getTargetY() << "y, " << this->getTargetZ() << "z" << std::endl;
        }

        if(this->getTargetZ() == 0)
        {
          this->setTargetId(Engine::getInstance().getWorld()->getCellId(this->getTargetX(), this->getTargetY(), this->getTargetZ()));   

          Cell* chosenCell;  
          targetTravelTime = 0;      

          if(communicationsRange == -1)
          {
              chosenCell = Engine::getInstance().getWorld()->getCells().at(this->getTargetId());
          }
          if(communicationsRange>0)
          {
              chosenCell = cells.at(this->getTargetId());
              chosenCell->setLastWeeedsSeen(-1);
          }
          if(DEBUG_THIS  && (id == testingId || id == testingId_2))
          {
          std::cout << "Agent " << this->getId() << " has picked "<< this->getTargetX() << "x + " << this->getTargetY() << "y in Cell " 
          << chosenCell->getId() << " from " << knowledgeBaseLocation << " knowledge base as Target, which previously had " << cells.at(this->getTargetId())->isTargetOf.size() << " agents that had it as target." << std::endl;
          }

          chosenCell->isTargetOf.push_back(this->getId()); 
          if(communicationsRange == -1)
          {
          //std::cout << "Agent " << this->getId() << " pushing isTargetOf to cell " << chosenCell->getId() << std::endl;
          BroadcastCell(this, chosenCell);
          }
          if(communicationsRange>0)
          {BroadcastCell(this, chosenCell);} 
        }

      break;
    }
    case MOVE:
    {

      bool DEBUG_THIS = false;

      //std::cout << "I am moving" << std::endl;
      if(mesh != nullptr)
      {ChangeColor(movingColor);}

      if(DEBUG_THIS)//  && (id == testingId || id == testingId_2))
      {
        std::cout << "Agent " << this->getId() << " is at location " 
        << this->position.at(0) << "x, " << this->position.at(1) << "y, " << this->position.at(2) << "z" 
        << ". With cell " << this->getTargetId() << " and target coordinates " 
        << this->getTargetX() << "x, " << this->getTargetY() << "y, " << this->getTargetZ() << "z"
        << std::endl;
      }
      
      if(targetTravelTime<limitForTargetReselection)
      {
        targetTravelTime++;
      }
      else
      {
        forgetTarget();
      }
      

      std::array<float,3> nextPose = getNextPosition();
      if(!Engine::getInstance().moveAgentTo(nextPose.at(0),nextPose.at(1),nextPose.at(2),this->id)){
        std::cout << "I'M NOT MOVING" << std::endl;
        this->velocity.at(0) = 0;
        this->velocity.at(1) = 0;
      }

      break;
    }
    case SCAN:
	  {
      //std::cout << "Agent A: " << this->getId() << std::endl;

      bool DEBUG_SCAN = false;

      if(DEBUG_SCAN && testingId == id && false)
      {std::cout << "Getting cell at: " << position.at(0) << "x + " << position.at(1) << "y" << std::endl;}
      Cell* c = Engine::getInstance().getWorld()->getCell(this->position.at(0),this->position.at(1),this->position.at(2));
      c->numOfVisits++;
      if(DEBUG_SCAN && testingId == id && false)
      {std::cout << "Recieved cell: " << c->getId() << " at: " << c->getX() << "x + " << c->getY() << std::endl;}
      
      //std::cout << "Agent B: " << this->getId() << " current cell " << c->getId() << std::endl;

      this->velocity = {0.0,0.0,0.0};
      
      //std::cout << "Agent C: " << this->getId() << " Table size " << sizeof(Engine::getInstance().getWorld()->getSensorTable()) 
      //<< " cell value " << Engine::getInstance().getWorld()->getSensorTable()[13][0] << std::endl;
      //std::cout << "Agent D: " << this->getId() << " world cells remaining " << Engine::getInstance().getWorld()->remainingTasksToVisit.at(136)->getId() << std::endl;

      //Cell* cellToRemove = Engine::getInstance().getWorld()->remainingTasksToVisit.at(c->getId());

      //std::cout << "Agent E: " << this->getId() << " world cells remaining " << Engine::getInstance().getWorld()->remainingTasksToVisit.size() << std::endl;

      Engine::getInstance().getWorld()->remainingTasksToVisit.erase(c->getId());
      //Engine::getInstance().getWorld()->removeTaskToVisit(c->getId());

      //std::cout << "Agent F: " << this->getId() << std::endl;

      Cell* scanningCell;

      if(communicationsRange == -1)
      {scanningCell = c;}
      if(communicationsRange > 0)
      {
      unsigned targetId = c->getId();
      scanningCell = this->cells.at(targetId);
      scanningCell->lastTimeVisit = timeStep;    
      }        

      if(!scanningCell->isMapped())
      {            
      //std::cout << std::endl;    
      if(DEBUG_SCAN && testingId == id && true)
      {std::cout << "TS: " << timeStep << ". Agent " << this->getId() << " in location: " << getX() << "x " << getY() << "y" << ". Scanning cell " 
      << scanningCell->getId() << " in location " << scanningCell->getX() << "x " << scanningCell->getY() << "y" << std::endl;}
        
        //std::cout << "Agent F: " << this->getId() << std::endl;
        float weedsSeen = scanCurrentLocation(scanningCell, -1); 
        //std::cout << "Agent G: " << this->getId() << std::endl;      
        
        if(scanningCell->getResidual() < 0.27 )
        { 
          //if(scanningCell->getUtility()>0)
          //{std::cout<< "Cell " << scanningCell->getId() << " was mapped by agent " << this->getId()  << std::endl;}
          
          scanningCell->setMapped();
          Engine::getInstance().getWorld()->remainingTasksToMap.erase(scanningCell->getId());
          Engine::getInstance().getWorld()->remainingTasksIntoClusters.erase(scanningCell->getId());
          Engine::getInstance().getWorld()->getCells().at(scanningCell->getId())->setMapped();
          
          if(scanningCell->getBeacon() != 0 && currentInspectionStrategy == "rw")
          {
            if(communicationsRange == -1)
            {Engine::getInstance().getWorld()->beacons.erase(scanningCell->getId());}
            if(communicationsRange > 0)
            {this->beacons.erase(scanningCell->getId());}

            scanningCell->setBeacon(0);
          }
        }
        else
        {    // the cell is not yet mapped
          if(currentInspectionStrategy == "rw")
          {
            if(mesh != nullptr)
            {ChangeColor(scanningColor);}

            float beacon = weedsSeen/14;
            scanningCell->setBeacon(beacon);
            if(communicationsRange == -1)
            {
              Engine::getInstance().getWorld()->beacons.insert(std::make_pair<>(scanningCell->getId(), scanningCell));
              Engine::getInstance().getWorld()->beacons.at(scanningCell->getId()) = scanningCell;
            }
            if(communicationsRange > 0)
            {
              this->beacons.insert(std::make_pair<>(scanningCell->getId(), scanningCell));
              this->beacons.at(scanningCell->getId()) = scanningCell;
            }
            
            
            if(mesh != nullptr)
            {ChangeColor(scanningColor);}
          }      
        }                  
      }
      else
      {
      //; before adding the scancurrentlocation line there was only a ; in this else space
      float weedsSeen = scanCurrentLocation(scanningCell, -1);

      }
      
      //std::cout << "Agent " << this->getId() << " starting to broadcast scan results of cell " << scanningCell->getId() << std::endl;
      if(communicationsRange == -1)
      {
        BroadcastCell(this, scanningCell);
      }
      if(communicationsRange>0)
      {BroadcastCell(this, scanningCell);} 
      
      //std::cout << "Agent " << this->getId() << " visited cell " << scanningCell->getId() << ". But it was already mapped according to current KB" << std::endl;
      /*
      scanningCell->isTargetOf.clear();
      this->cells.at(scanningCell->getId())->isTargetOf.clear();
      this->targetId = -1;
      this->target = {-1,-1,-1};
      */
      forgetTarget();
      break;          
          
    }
  default:
  break;
  }
  

  return true;
  
  
};

bool Agent::isInBound(unsigned x, unsigned y)
{     
     return x >= 0 && y >= 0 && x < Engine::getInstance().getWorld()->getSize().at(0) && y < Engine::getInstance().getWorld()->getSize().at(1);;
}


/**
 * Perform a scan at the current location (that is supposed to be the targetLocation)
 */
float Agent::scanCurrentLocation(Cell* currentCell, int weedsReceived)
{ 
  //if(weedsReceived==-1)
  //{	assert(getTargetId() != -1);}

  bool DEBUG_THIS = false;

  // scan at current location and return the perceived cell
    //std::cout << std::endl;    
    //clear observationVector


    if(DEBUG_THIS && testingId == id)
    {
      for(unsigned l = 0; l < (13); l++)
      {
        for(unsigned k = 0; k < (13+1); k++)
        {
        std::cout << Engine::getInstance().getWorld()->getSensorTable()[k][l] << " ";    
        }
        std::cout << std::endl;
      }
    }

    currentCell->observationVector.fill(0);
    //compute observationVector using the current knowledge and the constant sensorTable
    if(DEBUG_THIS  && testingId == id)
    {std::cout << "Observation Vector: " << std::endl;}
    //std::cout << "Knowledge Vector: " << std::endl;
    for(unsigned l = 0; l < 13; l++ )
    {
        for(unsigned k = 0; k < 13+1; k++ )
        {
            //std::cout << "old observation: " << currentCell->observationVector[k] << std::endl;
            
            //std::cout<<"  TABLE   =  indice: "<<l<<" - "<<k<<"  "<<Engine::getInstance().getWorld()->getSensorTable()[l][k]<<std::endl;
            currentCell->observationVector[k] += currentCell->knowledgeVector[l]*Engine::getInstance().getWorld()->getSensorTable()[k][l]; //equation 3
            
            //if(DEBUG_THIS && testingId == id)
            //{std::cout << currentCell->observationVector[k] << " ";}                    
        }
        if(DEBUG_THIS && testingId == id)
        {std::cout << currentCell->observationVector[l] << " ";}  
        //if(DEBUG_THIS && testingId == id)
        //{std::cout << std::endl;}
    }
    if(DEBUG_THIS && testingId == id)
    {std::cout << std::endl; }

    //get amount of weeds seen by sensor in current observation
    unsigned weedsSeen;
    if(weedsReceived >= 0)
    {
      weedsSeen = weedsReceived;
    }
    else
    {
      double random = RandomGenerator::getInstance().nextFloat(1);
      for (unsigned i = 0; i < (13+1); i++)
      {  
        if(DEBUG_THIS && testingId == id)
        {std::cout << "Random " << random << " Table value " << Engine::getInstance().getWorld()->getSensorTable()[13][currentCell->getUtility()] << std::endl;}    


        random -= Engine::getInstance().getWorld()->getSensorTable()[i][currentCell->getUtility()];


        if(random <= 0)
        {
          weedsSeen = i;
          currentCell->setLastWeeedsSeen(weedsSeen);
          break;
        }
      }  
    }
    


    currentCell->residual_uncertainty = 0.0;
    float entr = 0;

    if(DEBUG_THIS  && testingId == id)
    {std::cout << "Knowledge Vector for cell with " << currentCell->getUtility() << " weeds and " << weedsSeen << " seen weeds:" << std::endl;}
    
    for(unsigned i = 0; i<13; i++)
    {
      currentCell->knowledgeVector[i] = currentCell->knowledgeVector[i] * Engine::getInstance().getWorld()->getSensorTable()[weedsSeen][i] 
                                  / currentCell->observationVector[weedsSeen]; //current cell, equation 5
      
      if(DEBUG_THIS  && testingId == id)
      {std::cout << currentCell->knowledgeVector[i] << " ";}   
      //if i-th element is != 0  ---> calculate H(c)
     
      if(currentCell->knowledgeVector[i] != 0)
      {
        entr +=  currentCell->knowledgeVector[i]*(std::log(currentCell->knowledgeVector[i])); //equation 6
      }
    }
    
    if(DEBUG_THIS  && testingId == id)
    {std::cout << std::endl;}
    
    
    //std::cout << "total residual uncertainty " << -entr << " of cell " << currentCell->getId() << std::endl; 
    //store H(c) for the current cell
    currentCell->residual_uncertainty = -entr;

    if(DEBUG_THIS  && testingId == id)
    {std::cout << "Residual Entropy " << -entr << std::endl << std::endl;}
    
    if(currentCell->residual_uncertainty<0.27 && !(currentCell->isMapped()))
    {
      std::stringstream scanReport;


      
      scanReport << currentCell->getId() << " " << currentCell->getUtility() << " " << weedsSeen << " " << this->getId() << " " << timeStep << " " << weedsSeen << " ";

      for(float f : currentCell->knowledgeVector)
      {
        scanReport << f << " ";
      }
      for(float f : currentCell->observationVector)
      {
        scanReport << f << " ";
      }
      std::string outString;
      outString = scanReport.str();
      Engine::getInstance().WriteKnowledgeBasesFile(outString);
      
    }



    return weedsSeen;
  
};

bool Agent::getInfo(std::stringstream& ss){
  ss << this->getId() << ':' << ' ' << this->getX() << ' ' << this->getY() << ' ' << this->getZ()+2 << '\n';
  return true;
};



void Agent::ChangeColor(glm::vec4 inColor)
{
    if(mesh!=nullptr)
    {
      //if(this->getId()!=testingId)this->getId()<40

      mesh->SetCurrentColor(inColor);
      
      if(this->getId()==testingId)// || (this->getId()<48 && this->getId()>25))
      {
        mesh->SetCurrentColor(testColor);
      }
    }
}



