#include "Math/Vector3D.h"
#include "Math/Vector4D.h"
#include "ROOT/TDataFrame.hxx"
#include "TFile.h"
#include "TMath.h"
#include "TTree.h"
#include "TRandom3.h"
#include <cassert>
#include <iostream>


using FourVector = ROOT::Math::XYZTVector;
using FourVectors = std::vector<FourVector>;
using CylFourVector = ROOT::Math::RhoEtaPhiVector;

void getTracks(FourVectors& tracks) {
   static TRandom3 R(1);
   const double M = 0.13957;  // set pi+ mass
   auto nPart = R.Poisson(5);
   tracks.clear();
   tracks.reserve(nPart);
   for (int i = 0; i < nPart; ++i) {
      double px = R.Gaus(0,10);
      double py = R.Gaus(0,10);
      double pt = sqrt(px*px +py*py);
      double eta = R.Uniform(-3,3);
      double phi = R.Uniform(0.0 , 2*TMath::Pi() );
      CylFourVector vcyl( pt, eta, phi);
      // set energy
      double E = sqrt( vcyl.R()*vcyl.R() + M*M);
      FourVector q( vcyl.X(), vcyl.Y(), vcyl.Z(), E);
      // fill track vector
      tracks.emplace_back(q);
   }
}

// A simple helper function to fill a test tree and save it to file
// This makes the example stand-alone
void FillTree(const char* filename, const char* treeName) {
   TFile f(filename,"RECREATE");
   TTree t(treeName,treeName);
   double b1;
   int b2;
   float b3;
   float b4;
   std::vector<FourVector> tracks;
   std::vector<double> dv {-1,2,3,4};
   std::vector<float> sv {-1,2,3,4};
   std::list<int> sl {1,2,3,4};
   t.Branch("b1", &b1);
   t.Branch("b2", &b2);
   t.Branch("b3", &b3);
   t.Branch("b4", &b4);
   t.Branch("tracks", &tracks);
   t.Branch("dv", &dv);
   t.Branch("sl", &sl);
   t.Branch("sv", &sv);

   for(int i = 0; i < 20; ++i) {
      b1 = i;
      b2 = i*i;
      b3 = sqrt(i*i*i);
      b4 = i;
      getTracks(tracks);
      dv.emplace_back(i);
      sl.emplace_back(i);
      sv.emplace_back(i * 0.5);
      t.Fill();
   }
   t.Write();
   f.Close();
   return;
}

template<class T>
void CheckRes(const T& v, const T& ref, const char* msg) {
   if (v!=ref) {
      std::cerr << "***FAILED*** " << msg << std::endl;
   }
}

int main() {
   // Prepare an input tree to run on
   auto fileName = "test_misc.root";
   auto treeName = "myTree";
   FillTree(fileName,treeName);

   TFile f(fileName);
   // Define data-frame
   ROOT::Experimental::TDataFrame d(treeName, &f);
   // ...and two dummy filters
   auto ok = []() { return true; };
   auto ko = []() { return false; };

   // TEST 1: no-op filter and Run
   d.Filter(ok, {}).Foreach([](double x) { std::cout << x << std::endl; }, {"b1"});

   // TEST 2: Forked actions
   // always apply first filter before doing three different actions
   auto dd = d.Filter(ok, {});
   dd.Foreach([](double x) { std::cout << x << " "; }, {"b1"});
   dd.Foreach([](int y) { std::cout << y << std::endl; }, {"b2"});
   auto c = dd.Count();
   // ... and another filter-and-foreach
   auto ddd = dd.Filter(ko, {});
   ddd.Foreach([]() { std::cout << "ERROR" << std::endl; }, {});
   auto cv = *c;
   std::cout << "c " << cv << std::endl;
   CheckRes(cv,20U,"Forked Actions");

   // TEST 3: default branches
   ROOT::Experimental::TDataFrame d2(treeName, &f, {"b1"});
   auto d2f = d2.Filter([](double b1) { return b1 < 5; }).Filter(ok, {});
   auto c2 = d2f.Count();
   d2f.Foreach([](double b1) { std::cout << b1 << std::endl; });
      auto c2v = *c2;
   std::cout << "c2 " << c2v << std::endl;
   CheckRes(c2v,5U,"Default branches");

   // TEST 4: execute Run lazily and implicitly
   ROOT::Experimental::TDataFrame d3(treeName, &f, {"b1"});
   auto d3f = d3.Filter([](double b1) { return b1 < 4; }).Filter(ok, {});
   auto c3 = d3f.Count();
   auto c3v = *c3;
   std::cout << "c3 " << c3v << std::endl;
   CheckRes(c3v,4U,"Execute Run lazily and implicitly");

   // TEST 5: non trivial branch
   ROOT::Experimental::TDataFrame d4(treeName, &f, {"tracks"});
   auto d4f = d4.Filter([](FourVectors const & tracks) { return tracks.size() > 7; });
   auto c4 = d4f.Count();
   auto c4v = *c4;
   std::cout << "c4 " << c4v << std::endl;
   CheckRes(c4v,1U,"Non trivial test");

   // TEST 6: Create a histogram
   ROOT::Experimental::TDataFrame d5(treeName, &f, {"b2"});
   auto h1 = d5.Histo1D();
   auto h2 = d5.Histo1D("b1");
   TH1F dvHisto("dvHisto","The DV histo", 64, -8, 8);
   auto h3 = d5.Histo1D(std::move(dvHisto),"dv");
   auto h4 = d5.Histo1D<std::list<int>>("sl");
   std::cout << "Histo1: nEntries " << h1->GetEntries() << std::endl;
   std::cout << "Histo2: nEntries " << h2->GetEntries() << std::endl;
   std::cout << "Histo3: nEntries " << h3->GetEntries() << std::endl;
   std::cout << "Histo4: nEntries " << h4->GetEntries() << std::endl;

   // TEST 7: AddBranch
   ROOT::Experimental::TDataFrame d6(treeName, &f);
   auto r6 = d6.AddBranch("iseven", [](int b2) { return b2 % 2 == 0; }, {"b2"})
               .Filter([](bool iseven) { return iseven; }, {"iseven"})
               .Count();
   auto c6v = *r6;
   std::cout << c6v << std::endl;
   CheckRes(c6v, 10U, "AddBranch");

   // TEST 8: AddBranch with default branches, filters, non-trivial types
   ROOT::Experimental::TDataFrame d7(treeName, &f, {"tracks"});
   auto dd7 = d7.Filter([](int b2) { return b2 % 2 == 0; }, {"b2"})
                 .AddBranch("ptsum", [](FourVectors const & tracks) {
                    double sum = 0;
                    for(auto& track: tracks)
                       sum += track.Pt();
                    return sum; });
   auto c7 = dd7.Count();
   auto h7 = dd7.Histo1D("ptsum");
   auto c7v = *c7;
   CheckRes(c7v, 10U, "AddBranch complicated");
   std::cout << "AddBranch Histo entries: " << h7->GetEntries() << std::endl;
   std::cout << "AddBranch Histo mean: " << h7->GetMean() << std::endl;

   // TEST 9: Get minimum, maximum, mean
   ROOT::Experimental::TDataFrame d8(treeName, &f, {"b2"});
   auto min_b2 = d8.Min();
   auto min_dv = d8.Min("dv");
   auto max_b2 = d8.Max();
   auto max_dv = d8.Max("dv");
   auto mean_b2 = d8.Mean();
   auto mean_dv = d8.Mean("dv");

   auto min_b2v = *min_b2;
   auto min_dvv = *min_dv;
   auto max_b2v = *max_b2;
   auto max_dvv = *max_dv;
   auto mean_b2v = *mean_b2;
   auto mean_dvv = *mean_dv;

   CheckRes(min_b2v, 0., "Min of ints");
   CheckRes(min_dvv, -1., "Min of vector<double>");
   CheckRes(max_b2v, 361., "Max of ints");
   CheckRes(max_dvv, 19., "Max of vector<double>");
   CheckRes(mean_b2v, 123.5, "Mean of ints");
   CheckRes(mean_dvv, 5.1379310344827588963, "Mean of vector<double>");

   std::cout << "Min b2: " << *min_b2 << std::endl;
   std::cout << "Min dv: " << *min_dv << std::endl;
   std::cout << "Max b2: " << *max_b2 << std::endl;
   std::cout << "Max dv: " << *max_dv << std::endl;
   std::cout << "Mean b2: " << *mean_b2 << std::endl;
   std::cout << "Mean dv: " << *mean_dv << std::endl;

   // TEST 10: Get a full column
   ROOT::Experimental::TDataFrame d9(treeName, &f, {"tracks"});
   auto dd9 = d9.Filter([](int b2) { return b2 % 2 == 0; }, {"b2"})
                 .AddBranch("ptsum", [](FourVectors const & tracks) {
                    double sum = 0;
                    for(auto& track: tracks)
                       sum += track.Pt();
                    return sum; });
   auto b2List = dd9.Take<int>("b2");
   auto ptsumVec = dd9.Take<double, std::vector<double>>("ptsum");

   for (auto& v : b2List) { // Test also the iteration without dereferencing
      std::cout << v << std::endl;
   }

   for (auto& v : *ptsumVec) {
      std::cout << v << std::endl;
   }

   // TEST 11: Re-hang action to TDataFrameProxy after running
   ROOT::Experimental::TDataFrame d10(treeName, &f, {"tracks"});
   auto d10f = d10.Filter([](FourVectors const & tracks) { return tracks.size() > 2; });
   auto c10 = d10f.Count();
   std::cout << "Count for the first run is " << *c10 << std::endl;
   auto d10f_2 = d10f.Filter([](FourVectors const & tracks) { return tracks.size() < 5; });
   auto c10_2 = d10f_2.Count();
   std::cout << "Count for the second run after adding a filter is " << *c10_2 << std::endl;
   std::cout << "Count for the first run was " << *c10 << std::endl;

   // TEST 12: Test a frame which goes out of scope
   auto l = [](FourVectors const & tracks) { return tracks.size() > 2; };
   auto giveMeFilteredDF = [&](){
      ROOT::Experimental::TDataFrame d11(treeName, &f, {"tracks"});
      auto a = d11.Filter(l);
      return a;
   };
   auto filteredDF = giveMeFilteredDF();
   // Prevent bombing
   try {
      auto c11 = filteredDF.Count();
   } catch (const std::runtime_error& e) {
      std::cout << "Exception caught: the dataframe went out of scope when booking an action." << std::endl;
   }

   // TEST 13: an action result pointer goes out of scope and the chain is ran
   ROOT::Experimental::TDataFrame d11(treeName, &f);
   auto d11c = d.Count();
   {
      auto d11c2 = d.Count();
      auto d11c4 = d.Count();
      auto d11c3 = d.Count();
      auto d11c5 = d.Count();
      auto d11c6 = d.Count();
      auto d11c7 = d.Count();
      auto d11c8 = d.Count();
      auto d11c9 = d.Count();
      auto d11c10 = d.Count();
      auto d11c11 = d.Count();
   }
   std::cout << "Count with action pointers which went out of scope: " << *d11c << std::endl;

   // TEST 14: fill 1D histograms
   ROOT::Experimental::TDataFrame d12(treeName, &f, {"b1","b2"});
   auto wh1 = d12.Histo1D<double, int>();
   auto wh2 = d12.Histo1D<std::vector<double>, std::list<int>>("dv","sl");
   std::cout << "Wh1 Histo entries: " << wh1->GetEntries() << std::endl;
   std::cout << "Wh1 Histo mean: " << wh1->GetMean() << std::endl;
   std::cout << "Wh2 Histo entries: " << wh2->GetEntries() << std::endl;
   std::cout << "Wh2 Histo mean: " << wh2->GetMean() << std::endl;

   // TEST 15: fill 2D histograms
   ROOT::Experimental::TDataFrame d13(treeName, &f, {"b1","b2","b3"});
   auto h12d = d13.Histo2D<double, int>(TH2F("h1","",64,0,1024,64,0,1024));
   auto h22d = d13.Histo2D<std::vector<double>, std::list<int>>(TH2F("h2","",64,0,1024,64,0,1024),"dv","sl");
   auto h32d = d13.Histo2D<double, int, float>(TH2F("h3","",64,0,1024,64,0,1024));
   std::cout << "h12d Histo entries: " << h12d->GetEntries() << std::endl;
   std::cout << "h22d Histo entries: " << h22d->GetEntries() << std::endl;
   std::cout << "h32d Histo entries: " << h32d->GetEntries() << " sum of weights: " << h32d->GetSumOfWeights() << std::endl;

   // TEST 15: fill 3D histograms
   ROOT::Experimental::TDataFrame d14(treeName, &f, {"b1","b2","b3","b4"});
   auto h13d = d14.Histo3D<double, int, float>(TH3F("h4","",64,0,1024,64,0,1024,64,0,1024));
   auto h23d = d14.Histo3D<std::vector<double>,
                           std::list<int>,
                           std::vector<float>>(TH3F("h5","",64,0,1024,64,0,1024,64,0,1024),"dv","sl","sv");
   auto h33d = d14.Histo3D<double, int, float, float>(TH3F("h6","",64,0,1024,64,0,1024,64,0,1024));
   std::cout << "h13d Histo entries: " << h13d->GetEntries() << std::endl;
   std::cout << "h23d Histo entries: " << h23d->GetEntries() << std::endl;
   std::cout << "h33d Histo entries: " << h33d->GetEntries() << " sum of weights: " << h33d->GetSumOfWeights() << std::endl;
   return 0;
}

void test_misc(){main();}