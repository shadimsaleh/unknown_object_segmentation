/**
 *  Copyright (C) 2012  
 *    Andreas Richtsfeld, Johann Prankl, Thomas Mörwald
 *    Automation and Control Institute
 *    Vienna University of Technology
 *    Gusshausstraße 25-29
 *    1170 Vienn, Austria
 *    ari(at)acin.tuwien.ac.at
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see http://www.gnu.org/licenses/
 */

/**
 * @file Graph.cpp
 * @author Andreas Richtsfeld
 * @date July 2011
 * @version 0.1
 * @brief Create graph from realtions.
 */

#include <unknown_objects_segmentation/Graph.h>

#ifndef GC_DEBUG
#define GC_DEBUG false
#endif

namespace gc
{

template<typename T1,typename T2>
inline T1 Dot3(const T1 v1[3], const T2 v2[3])
{
  return v1[0]*v2[0] + v1[1]*v2[1] + v1[2]*v2[2];
}
 
template<typename T1,typename T2, typename T3>
inline void Add3(const T1 v1[3], const T2 v2[3], T3 r[3])
{
  r[0] = v1[0]+v2[0];
  r[1] = v1[1]+v2[1];
  r[2] = v1[2]+v2[2];
}

/**
 * @brief Constructor of Graph
 */
Graph::Graph()
{
  nodes = 0;
  relations.clear();
}
  
/**
 * @brief Constructor of Graph
 */
Graph::Graph(unsigned nrNodes, std::vector<surface::Relation> &rel)
{
  nodes = nrNodes;
  relations = rel;
}


/**
 * @brief Destructor of GraphCut
 */
Graph::~Graph()
{}


/**
 * @brief Build graph from results of the SVM-Predictor.
 * @param e Vector of edges
 * @param num_edges Number of created edges.
 */
void Graph::BuildFromSVM(std::vector<gc::Edge> &e, unsigned &num_edges)
{
  if(GC_DEBUG) {
    printf("[Graph::BuildFromSVM] Number of nodes: %u\n", nodes);
    for(unsigned i=0; i<relations.size(); i++)
      printf("[Graph::BuildFromSVM] Relation %u: %u-%u\n", i, relations[i].id_0, relations[i].id_1);
  }
      
  // Connectivity check for graph
  for(unsigned i=1; i<nodes; i++) {
    bool node_found = false;
    for(unsigned j=0; j<relations.size(); j++)
      if(relations[j].id_0 == 0 && relations[j].id_1 == i)
        node_found = true;
    if(!node_found) {
      if(GC_DEBUG) 
        printf("[Graph::BuildFromSVM] Warning: Node without relation: Add relation: %u-%u.\n", 0, i);

      surface::Relation r;
      r.id_0 = 0;
      r.id_1 = i;
      r.groundTruth = -1;
      r.type = 1;
      r.rel_probability.push_back(1.0);
      r.rel_probability.push_back(0.0);
      relations.push_back(r);
    }
  }
    
  if(GC_DEBUG) {
    for(unsigned i=0; i<relations.size(); i++)
      printf("[Graph::BuildFromSVM] Relation %u: %u-%u\n", i, relations[i].id_0, relations[i].id_1);
  }    

  for(unsigned i=0; i< relations.size(); i++) {
    gc::Edge e;
    e.a = relations[i].id_0;
    e.b = relations[i].id_1;
    e.type = 1;
    e.w = relations[i].rel_probability[0];
    if(GC_DEBUG)
      printf("[Graph::BuildFromSVM] New edge (type: %i): %i-%i: %8.8f\n", e.type, e.a, e.b, e.w);
    edges.push_back(e);
  }
    
  num_edges = edges.size();
  e = edges;
  
  if(GC_DEBUG)
    printf("[Graph::BuildFromSVM] Created %d edges from %d relations\n", edges.size(), relations.size());

}

/**
 * @brief Build graph from point cloud with 4-neighborhood.
 * @param pcl_cloud
 * @param e Vector of edges
 * @param num_edges Number of created edges.
 */
void Graph::BuildFromPointCloud(pcl::PointCloud<pcl::PointXYZRGB>::Ptr &_pcl_cloud,
                                pcl::PointCloud<pcl::Normal>::Ptr &_normals,
                                std::vector<gc::Edge> &e, unsigned &num_edges)
{
  float z_adapt = 0.01;
  pcl_cloud = _pcl_cloud;
  normals = _normals;
  edges.clear();
  
  // normalization of curvature
  float max_curv = 0.0;
  for(unsigned i=0; i<normals->points.size(); i++)
    if(!isnan(pcl_cloud->points[i].z))
      if(normals->points[i].curvature > max_curv)
        max_curv = normals->points[i].curvature;
  printf("[Graph::BuildFromPointCloud] Max curvature: %5.5f\n", max_curv);
      
  // pre-calculate color distance
  float color_dist[pcl_cloud->points.size()][4];
  float max_color = 0.0f;
// #pragma omp parallel for
  for(unsigned row=0; row < pcl_cloud->height-1; row++) {
    for(unsigned col=0; col < pcl_cloud->width-1; col++) {
      int idx = GetIdx(col, row);
      pcl::PointXYZRGB pt_0 = pcl_cloud->points[idx];                                           // Check also the distance of the points!
      pcl::PointXYZRGB pt_1 = pcl_cloud->points[idx+1];
      color_dist[idx][0] = sqrt(((float)pt_0.r/255.-(float)pt_1.r/255.)*((float)pt_0.r/255.-(float)pt_1.r/255.) + 
                                ((float)pt_0.g/255.-(float)pt_1.g/255.)*((float)pt_0.g/255.-(float)pt_1.g/255.) +
                                ((float)pt_0.b/255.-(float)pt_1.b/255.)*((float)pt_0.b/255.-(float)pt_1.b/255.));
      if(color_dist[idx][0] > max_color)
        max_color = color_dist[idx][0];
// printf("color dist[0]: %4.3f\n", color_dist[idx][0]);
      pt_0 = pcl_cloud->points[idx];
      pt_1 = pcl_cloud->points[idx+pcl_cloud->width];
      color_dist[idx][1] = sqrt(((float)pt_0.r/255.-(float)pt_1.r/255.)*((float)pt_0.r/255.-(float)pt_1.r/255.) + 
                                ((float)pt_0.g/255.-(float)pt_1.g/255.)*((float)pt_0.g/255.-(float)pt_1.g/255.) +
                                ((float)pt_0.b/255.-(float)pt_1.b/255.)*((float)pt_0.b/255.-(float)pt_1.b/255.));
      if(color_dist[idx][1] > max_color)
        max_color = color_dist[idx][1];
// printf("color dist[1]: %4.3f\n", color_dist[idx][1]);
      pt_0 = pcl_cloud->points[idx];
      pt_1 = pcl_cloud->points[idx+pcl_cloud->width+1];
      color_dist[idx][2] = sqrt(((float)pt_0.r/255.-(float)pt_1.r/255.)*((float)pt_0.r/255.-(float)pt_1.r/255.) + 
                                ((float)pt_0.g/255.-(float)pt_1.g/255.)*((float)pt_0.g/255.-(float)pt_1.g/255.) +
                                ((float)pt_0.b/255.-(float)pt_1.b/255.)*((float)pt_0.b/255.-(float)pt_1.b/255.));
      if(color_dist[idx][2] > max_color)
        max_color = color_dist[idx][2];
// printf("color dist[2]: %4.3f\n", color_dist[idx][2]);
      if(col != 0) {
        pt_0 = pcl_cloud->points[idx];
        pt_1 = pcl_cloud->points[idx+pcl_cloud->width-1];
        color_dist[idx][3] = sqrt(((float)pt_0.r/255.-(float)pt_1.r/255.)*((float)pt_0.r/255.-(float)pt_1.r/255.) + 
                                  ((float)pt_0.g/255.-(float)pt_1.g/255.)*((float)pt_0.g/255.-(float)pt_1.g/255.) +
                                  ((float)pt_0.b/255.-(float)pt_1.b/255.)*((float)pt_0.b/255.-(float)pt_1.b/255.));
      if(color_dist[idx][3] > max_color)
        max_color = color_dist[idx][3];
      }
      else
        color_dist[idx][3] = 1.0;
// printf("color dist[3]: %4.3f\n", color_dist[idx][3]);
    }
  }
  
  
//   /// YUV conversion
//   for(unsigned i=0; i < pcl_cloud->points.size(); i++) {
//     pcl::PointXYZRGB pt_o = pcl_cloud->points[i];
//     pcl::PointXYZRGB pt_n;
//     pt_n.r =  (0.257 * pt_o.r) + (0.504 * pt_o.g) + (0.098 * pt_o.b) + 16;     // R = Y
//     pt_n.g = -(0.148 * pt_o.r) - (0.291 * pt_o.g) + (0.439 * pt_o.b) + 128;    // G = U
//     pt_n.b =  (0.439 * pt_o.r) - (0.368 * pt_o.g) - (0.071 * pt_o.b) + 128;    // B = V
//   }
//   
//     // pre-calculate color distance
//   float color_dist[pcl_cloud->points.size()][4];
//   float max_color = 0.0f;
// // #pragma omp parallel for
//   for(unsigned row=0; row < pcl_cloud->height-1; row++) {
//     for(unsigned col=0; col < pcl_cloud->width-1; col++) {
//       int idx = GetIdx(col, row);
//       pcl::PointXYZRGB pt_0 = pcl_cloud->points[idx];                                           // Check also the distance of the points!
//       pcl::PointXYZRGB pt_1 = pcl_cloud->points[idx+1];
//       color_dist[idx][0] = sqrt(((float)pt_0.g/255.-(float)pt_1.g/255.)*((float)pt_0.g/255.-(float)pt_1.g/255.) +
//                                 ((float)pt_0.b/255.-(float)pt_1.b/255.)*((float)pt_0.b/255.-(float)pt_1.b/255.));
//       if(color_dist[idx][0] > max_color)
//         max_color = color_dist[idx][0];
// // printf("color dist[0]: %4.3f\n", color_dist[idx][0]);
//       pt_0 = pcl_cloud->points[idx];
//       pt_1 = pcl_cloud->points[idx+pcl_cloud->width];
//       color_dist[idx][1] = sqrt(((float)pt_0.g/255.-(float)pt_1.g/255.)*((float)pt_0.g/255.-(float)pt_1.g/255.) +
//                                 ((float)pt_0.b/255.-(float)pt_1.b/255.)*((float)pt_0.b/255.-(float)pt_1.b/255.));
//       if(color_dist[idx][1] > max_color)
//         max_color = color_dist[idx][1];
// // printf("color dist[1]: %4.3f\n", color_dist[idx][1]);
//       pt_0 = pcl_cloud->points[idx];
//       pt_1 = pcl_cloud->points[idx+pcl_cloud->width+1];
//       color_dist[idx][2] = sqrt(((float)pt_0.g/255.-(float)pt_1.g/255.)*((float)pt_0.g/255.-(float)pt_1.g/255.) +
//                                 ((float)pt_0.b/255.-(float)pt_1.b/255.)*((float)pt_0.b/255.-(float)pt_1.b/255.));
//       if(color_dist[idx][2] > max_color)
//         max_color = color_dist[idx][2];
// // printf("color dist[2]: %4.3f\n", color_dist[idx][2]);
//       if(col != 0) {
//         pt_0 = pcl_cloud->points[idx];
//         pt_1 = pcl_cloud->points[idx+pcl_cloud->width-1];
//         color_dist[idx][3] = sqrt(((float)pt_0.g/255.-(float)pt_1.g/255.)*((float)pt_0.g/255.-(float)pt_1.g/255.) +
//                                   ((float)pt_0.b/255.-(float)pt_1.b/255.)*((float)pt_0.b/255.-(float)pt_1.b/255.));
//       if(color_dist[idx][3] > max_color)
//         max_color = color_dist[idx][3];
//       }
//       else
//         color_dist[idx][3] = 1.0;
// // printf("color dist[3]: %4.3f\n", color_dist[idx][3]);
//     }
//   }
  
  
  
  // Create Graph
  for(unsigned row=0; row < pcl_cloud->height-1; row++) {                       /// TODO Könnte man auch parallelisieren
    for(unsigned col=0; col < pcl_cloud->width-1; col++) {
      int idx = GetIdx(col, row);
      
      /// RIGHT
      gc::Edge e;
      e.a = idx;
      e.b = idx+1;
      e.type = 1;
      e.w = color_dist[idx][0]/max_color;
// printf("color distance: %4.3f\n", e.w);      
      // normals angle 
      double angle = 0.0;
      if(isnan(pcl_cloud->points[idx].z))
        angle = 1.57;
      else
        angle = acos( Dot3(&normals->points[idx].normal[0], &normals->points[idx+1].normal[0]) );
      if(!isnan(angle))
        e.w2 = angle;
      else
        e.w2 = 1.57;
      
      float z_dist = fabs(pcl_cloud->points[idx].z - pcl_cloud->points[idx+1].z);
      if(!isnan(pcl_cloud->points[idx].z) && !isnan(pcl_cloud->points[idx+1].z) && z_dist < (z_adapt*pcl_cloud->points[idx].z))
        edges.push_back(e);

      /// BOTTOM
      e.a = idx;
      e.b = idx+pcl_cloud->width;
      e.type = 1;
      e.w = color_dist[idx][1]/max_color;
// printf("color distance: %4.3f\n", e.w);      
      
      // normals angle 
      angle = 0.0;
      if(isnan(pcl_cloud->points[idx].z))
        angle = 1.57;
      else
        angle = acos( Dot3(&normals->points[idx].normal[0], &normals->points[idx+pcl_cloud->width].normal[0]) );
      if(!isnan(angle))
        e.w2 = angle;
      else
        e.w2 = 1.57;      

      z_dist = fabs(pcl_cloud->points[idx].z - pcl_cloud->points[idx+pcl_cloud->width].z);
      if(!isnan(pcl_cloud->points[idx].z) && !isnan(pcl_cloud->points[idx+pcl_cloud->width].z) && z_dist < (z_adapt*pcl_cloud->points[idx].z))
        edges.push_back(e);
      
      /// BOTTOM-RIGHT
      e.a = idx;
      e.b = idx+pcl_cloud->width+1;
      e.type = 1;
      e.w = color_dist[idx][2]/max_color;
// printf("color distance: %4.3f\n", e.w);      
      
      // normals angle 
      angle = 0.0;
      if(isnan(pcl_cloud->points[idx].z))
        angle = 1.57;
      else
        angle = acos( Dot3(&normals->points[idx].normal[0], &normals->points[idx+pcl_cloud->width+1].normal[0]) );
      if(!isnan(angle))
        e.w2 = angle;
      else
        e.w2 = 1.57;      
      
      z_dist = fabs(pcl_cloud->points[idx].z - pcl_cloud->points[idx+pcl_cloud->width+1].z);
      if(!isnan(pcl_cloud->points[idx].z) && !isnan(pcl_cloud->points[idx+pcl_cloud->width+1].z) && z_dist < (z_adapt*pcl_cloud->points[idx].z))
        edges.push_back(e);
      
      /// BOTTOM-LEFT
      if(idx%pcl_cloud->width != 0) {
        e.a = idx;
        e.b = idx+pcl_cloud->width-1;
        e.type = 1;
        e.w = color_dist[idx][3]/max_color;
// printf("color distance: %4.3f\n", e.w);      
       
        // normals angle 
        angle = 0.0;
        if(isnan(pcl_cloud->points[idx].z))
          angle = 1.57;
        else
          angle = acos( Dot3(&normals->points[idx].normal[0], &normals->points[idx+pcl_cloud->width-1].normal[0]) );
        if(!isnan(angle))
          e.w2 = angle;
        else
          e.w2 = 1.57;      

        z_dist = fabs(pcl_cloud->points[idx].z - pcl_cloud->points[idx+pcl_cloud->width-1].z);
        if(!isnan(pcl_cloud->points[idx].z) && !isnan(pcl_cloud->points[idx+pcl_cloud->width-1].z) && z_dist < (z_adapt*pcl_cloud->points[idx].z))
          edges.push_back(e);
      }
      else {
        e.w = 1.0;
        z_dist = fabs(pcl_cloud->points[idx].z - pcl_cloud->points[idx+pcl_cloud->width-1].z);
        if(!isnan(pcl_cloud->points[idx].z) && !isnan(pcl_cloud->points[idx+pcl_cloud->width-1].z) && z_dist < (z_adapt*pcl_cloud->points[idx].z))
          edges.push_back(e);
      }
    }
  }
  num_edges = edges.size();
  e = edges;
  
  
/// HACK: OLD STUFF
/// curvature
//       float curv = normals->points[idx].curvature;
//       e.w = curv / max_curv;
//       if(isnan(pcl_cloud->points[idx].z) || z_dist > 0.004)
//         e.w = 1.0f;
/// curvature
//       curv = normals->points[idx].curvature;
//       e.w = curv * 10.;
//       if(isnan(pcl_cloud->points[idx].z) || z_dist > 0.004)
//         e.w = 1.0f;

}

} 











