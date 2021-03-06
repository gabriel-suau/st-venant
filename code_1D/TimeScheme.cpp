#include "TimeScheme.h"
#include "DataFile.h"
#include "Mesh.h"
#include "Function.h"
#include "FiniteVolume.h"

#include "Eigen/Eigen/Dense"
#include "Eigen/Eigen/Sparse"

#include <iostream>
#include <fstream>
#include <string>
#include <cmath>

TimeScheme::TimeScheme()
{
}

TimeScheme::TimeScheme(DataFile* DF, Mesh* mesh, Function* function, FiniteVolume* finVol):
  _DF(DF), _mesh(mesh), _function(function), _finVol(finVol), _Sol(_function->getInitialCondition()), _timeStep(DF->getTimeStep()), _initialTime(DF->getInitialTime()), _finalTime(DF->getFinalTime()), _currentTime(_initialTime)
{
}

void TimeScheme::Initialize(DataFile* DF, Mesh* mesh, Function* function, FiniteVolume* finVol)
{
  _DF = DF;
  _mesh = mesh;
  _function = function;
  _finVol = finVol;
  _Sol = _function->getInitialCondition();
  _timeStep = DF->getTimeStep();
  _initialTime = DF->getInitialTime();
  _finalTime = DF->getFinalTime();
  _currentTime = _initialTime;
}

void TimeScheme::saveCurrentSolution(std::string& fileName) const
{
  std::ofstream outputFile(fileName, std::ios::out);
  Eigen::VectorXd cellCenters (_mesh->getCellCenters());
  for (int i(0) ; i < _Sol.rows() ; ++i)
    {
      if (_Sol.row(i)(0) > 1e-14)
        {
          outputFile << cellCenters(i) << " " << _Sol(i,0) + _function->getTopography()(i,1) << " " << _Sol(i,1)/_Sol(i,0) << std::endl;
          // outputFile << cellCenters(i) << " " << _Sol(i,0) << " " << _Sol(i,1)/_Sol(i,0) << std::endl;
        }
      else
        {
          outputFile << cellCenters(i) << " " << _function->getTopography()(i,1) << " " << 0. << std::endl;
          // outputFile << cellCenters(i) << " " << 0. << " " << 0. << std::endl;
        }
    }
}

void TimeScheme::solve()
{
  // Logs de début
  std::cout << "====================================================================================================" << std::endl;
  std::cout << "Time loop..." << std::endl;

  // Variables pratiques
  int n(0);
  std::string resultsDir(_DF->getResultsDirectory());
  std::string fluxName(_finVol->getFluxName());
  
  // Sauvegarde la condition initiale
  std::string fileName(resultsDir + "/solution_" + fluxName + "_" + std::to_string(n) + ".txt");
  saveCurrentSolution(fileName);

  // Sauvegarde la topographie
  std::string topoFileName(resultsDir + "/topography.txt");
  std::ofstream topoFile(topoFileName, std::ios::out);
  for (int i(0) ; i < _Sol.rows() ; ++i)
    {
      topoFile << _function->getTopography()(i,0) << " " << _function->getTopography()(i,1) << std::endl;
    }
  
  // Boucle en temps
  while (_currentTime < _finalTime)
    {
      _function->buildSourceTerm(_Sol);
      _finVol->buildFluxVector(_Sol);
      oneStep();
      ++n;
      _currentTime += _timeStep;
      if (n % _DF->getSaveFrequency() == 0)
        {
          std::cout << "Saving solution at t = " << _currentTime << std::endl;
          std::string fileName(resultsDir + "/solution_" + fluxName + "_" + std::to_string(n/_DF->getSaveFrequency()) + ".txt");
          saveCurrentSolution(fileName); 
        }
    }
  
  // Logs de fin
  std::cout << termcolor::green << "TIMESCHEME::SUCCESS : Solved 1D St-Venant equations successfully !" << std::endl;
  std::cout << termcolor::reset << "====================================================================================================" << std::endl << std::endl;
}

ExplicitEuler::ExplicitEuler():
  TimeScheme()
{
}

ExplicitEuler::ExplicitEuler(DataFile* DF, Mesh* mesh, Function* function, FiniteVolume* finVol):
  TimeScheme(DF, mesh, function, finVol)
{
}

void ExplicitEuler::Initialize(DataFile* DF, Mesh* mesh, Function* function, FiniteVolume* finVol)
{
  _DF = DF;
  _mesh = mesh;
  _function = function;
  _finVol = finVol;
  _Sol.resize(mesh->getNumberOfCells(), 2);
  _timeStep = DF->getTimeStep();
  _initialTime = DF->getInitialTime();
  _finalTime = DF->getFinalTime();
  _currentTime = _initialTime; 
}

void ExplicitEuler::oneStep()
{
  // Récupération des trucs importants
  double dt(_timeStep);
  double dx(_mesh->getSpaceStep());
  int nCells(_mesh->getNumberOfCells());
  // Construction du terme source et du flux numérique
  Eigen::Matrix<double, Eigen::Dynamic, 2> source(_function->getSourceTerm());
  Eigen::Matrix<double, Eigen::Dynamic, 2> fluxVector(_finVol->getFluxVector());
  // Mise à jour de la solution sur chaque cellules
  for (int i(0) ; i < nCells ; ++i)
    {
      _Sol.row(i) += - dt*(fluxVector.row(i+1) - fluxVector.row(i))/dx + dt*source.row(i);
    }
}
