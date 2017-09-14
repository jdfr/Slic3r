#include "Print.hpp"
#include "BoundingBox.hpp"
#include "ClipperUtils.hpp"
#include "Fill/Fill.hpp"
#include "Flow.hpp"
#include "Geometry.hpp"
#include "SupportMaterial.hpp"
#include <algorithm>
#include <boost/filesystem.hpp>
#include <boost/lexical_cast.hpp>
#include <stdio.h>
#include "ClipperUtils.hpp"

namespace Slic3r {

template <class StepClass>
bool
PrintState<StepClass>::is_started(StepClass step) const
{
    return this->started.find(step) != this->started.end();
}

template <class StepClass>
bool
PrintState<StepClass>::is_done(StepClass step) const
{
    return this->done.find(step) != this->done.end();
}

template <class StepClass>
void
PrintState<StepClass>::set_started(StepClass step)
{
    this->started.insert(step);
}

template <class StepClass>
void
PrintState<StepClass>::set_done(StepClass step)
{
    this->done.insert(step);
}

template <class StepClass>
bool
PrintState<StepClass>::invalidate(StepClass step)
{
    bool invalidated = this->started.erase(step) > 0;
    this->done.erase(step);
    return invalidated;
}

template class PrintState<PrintStep>;
template class PrintState<PrintObjectStep>;


Print::Print()
:   total_used_filament(0),
    total_extruded_volume(0),
    useoutputpath(false)
{
}

Print::~Print()
{
    clear_objects();
    clear_regions();
}

bool
Print::useOutputPath()
{
  return useoutputpath;
}

std::string
Print::getOutputPath() {
  return outputpath;
}

void
Print::setOutputPath(std::string out)
{
  useoutputpath=true;
  outputpath=out;
}


void
Print::clear_objects()
{
    for (int i = this->objects.size()-1; i >= 0; --i)
        this->delete_object(i);

    this->clear_regions();
}

void
Print::delete_object(size_t idx)
{
    PrintObjectPtrs::iterator i = this->objects.begin() + idx;
    
    // before deleting object, invalidate all of its steps in order to 
    // invalidate all of the dependent ones in Print
    (*i)->invalidate_all_steps();
    
    // destroy object and remove it from our container
    delete *i;
    this->objects.erase(i);

    // TODO: purge unused regions
}

void
Print::reload_object(size_t idx)
{
    /* TODO: this method should check whether the per-object config and per-material configs
        have changed in such a way that regions need to be rearranged or we can just apply
        the diff and invalidate something.  Same logic as apply_config()
        For now we just re-add all objects since we haven't implemented this incremental logic yet.
        This should also check whether object volumes (parts) have changed. */
    
    // collect all current model objects
    ModelObjectPtrs model_objects;
    FOREACH_OBJECT(this, object) {
      if (((*object)->model_object())!=NULL) {
        model_objects.push_back((*object)->model_object());
      }
    }
    
    // remove our print objects
    this->clear_objects();
    
    // re-add model objects
    for (ModelObjectPtrs::iterator it = model_objects.begin(); it != model_objects.end(); ++it) {
        this->add_model_object(*it);
    }
}

bool
Print::reload_model_instances()
{
    bool invalidated = false;
    FOREACH_OBJECT(this, object) {
        if ((*object)->reload_model_instances()) invalidated = true;
    }
    return invalidated;
}

void
Print::clear_regions()
{
    for (int i = this->regions.size()-1; i >= 0; --i)
        this->delete_region(i);
}

PrintRegion*
Print::add_region()
{
    PrintRegion *region = new PrintRegion(this);
    regions.push_back(region);
    return region;
}

void
Print::delete_region(size_t idx)
{
    PrintRegionPtrs::iterator i = this->regions.begin() + idx;
    delete *i;
    this->regions.erase(i);
}

bool
Print::invalidate_state_by_config(const PrintConfigBase &config)
{
    const t_config_option_keys diff = this->config.diff(config);
    
    std::set<PrintStep> steps;
    std::set<PrintObjectStep> osteps;
    bool all = false;
    
    // this method only accepts PrintConfig option keys
    for (const t_config_option_key &opt_key : diff) {
        if (opt_key == "skirts"
            || opt_key == "skirt_height"
            || opt_key == "skirt_distance"
            || opt_key == "min_skirt_length"
            || opt_key == "ooze_prevention") {
            steps.insert(psSkirt);
        } else if (opt_key == "brim_width") {
            steps.insert(psBrim);
            steps.insert(psSkirt);
            osteps.insert(posSupportMaterial);
        } else if (opt_key == "brim_width"
            || opt_key == "interior_brim_width"
            || opt_key == "brim_connections_width") {
            steps.insert(psBrim);
            steps.insert(psSkirt);
        } else if (opt_key == "nozzle_diameter"
            || opt_key == "resolution"
            || opt_key == "z_steps_per_mm") {
            osteps.insert(posSlice);
        } else if (opt_key == "avoid_crossing_perimeters"
            || opt_key == "bed_shape"
            || opt_key == "bed_temperature"
            || opt_key == "between_objects_gcode"
            || opt_key == "bridge_acceleration"
            || opt_key == "bridge_fan_speed"
            || opt_key == "complete_objects"
            || opt_key == "cooling"
            || opt_key == "default_acceleration"
            || opt_key == "disable_fan_first_layers"
            || opt_key == "duplicate_distance"
            || opt_key == "end_gcode"
            || opt_key == "extruder_clearance_height"
            || opt_key == "extruder_clearance_radius"
            || opt_key == "extruder_offset"
            || opt_key == "extrusion_axis"
            || opt_key == "extrusion_multiplier"
            || opt_key == "fan_always_on"
            || opt_key == "fan_below_layer_time"
            || opt_key == "filament_colour"
            || opt_key == "filament_diameter"
            || opt_key == "filament_notes"
            || opt_key == "first_layer_acceleration"
            || opt_key == "first_layer_bed_temperature"
            || opt_key == "first_layer_speed"
            || opt_key == "first_layer_temperature"
            || opt_key == "gcode_arcs"
            || opt_key == "gcode_comments"
            || opt_key == "gcode_flavor"
            || opt_key == "infill_acceleration"
            || opt_key == "infill_first"
            || opt_key == "layer_gcode"
            || opt_key == "min_fan_speed"
            || opt_key == "max_fan_speed"
            || opt_key == "min_print_speed"
            || opt_key == "notes"
            || opt_key == "only_retract_when_crossing_perimeters"
            || opt_key == "output_filename_format"
            || opt_key == "perimeter_acceleration"
            || opt_key == "post_process"
            || opt_key == "pressure_advance"
            || opt_key == "printer_notes"
            || opt_key == "retract_before_travel"
            || opt_key == "retract_layer_change"
            || opt_key == "retract_length"
            || opt_key == "retract_length_toolchange"
            || opt_key == "retract_lift"
            || opt_key == "retract_lift_above"
            || opt_key == "retract_lift_below"
            || opt_key == "retract_restart_extra"
            || opt_key == "retract_restart_extra_toolchange"
            || opt_key == "retract_speed"
            || opt_key == "slowdown_below_layer_time"
            || opt_key == "spiral_vase"
            || opt_key == "standby_temperature_delta"
            || opt_key == "start_gcode"
            || opt_key == "temperature"
            || opt_key == "threads"
            || opt_key == "toolchange_gcode"
            || opt_key == "travel_speed"
            || opt_key == "use_firmware_retraction"
            || opt_key == "use_relative_e_distances"
            || opt_key == "vibration_limit"
            || opt_key == "wipe"
            || opt_key == "z_offset") {
            // these options only affect G-code export, so nothing to invalidate
        } else if (opt_key == "first_layer_extrusion_width") {
            osteps.insert(posPerimeters);
            osteps.insert(posInfill);
            osteps.insert(posSupportMaterial);
            steps.insert(psSkirt);
            steps.insert(psBrim);
        } else {
            // for legacy, if we can't handle this option let's invalidate all steps
            all = true;
            break;
        }
    }
    
    if (!diff.empty())
        this->config.apply(config, true);
    
    bool invalidated = false;
    if (all) {
        if (this->invalidate_all_steps())
            invalidated = true;
        
        for (PrintObject* object : this->objects)
            if (object->invalidate_all_steps())
                invalidated = true;
    } else {
        for (const PrintStep &step : steps)
            if (this->invalidate_step(step))
                invalidated = true;
    
        for (const PrintObjectStep &ostep : osteps)
            for (PrintObject* object : this->objects)
                if (object->invalidate_step(ostep))
                    invalidated = true;
    }
    
    return invalidated;
}

bool
Print::invalidate_step(PrintStep step)
{
    bool invalidated = this->state.invalidate(step);
    
    // propagate to dependent steps
    if (step == psSkirt) {
        this->invalidate_step(psBrim);
    }
    
    return invalidated;
}

bool
Print::invalidate_all_steps()
{
    // make a copy because when invalidating steps the iterators are not working anymore
    std::set<PrintStep> steps = this->state.started;
    
    bool invalidated = false;
    for (std::set<PrintStep>::const_iterator step = steps.begin(); step != steps.end(); ++step) {
        if (this->invalidate_step(*step)) invalidated = true;
    }
    return invalidated;
}

// returns true if an object step is done on all objects
// and there's at least one object
bool
Print::step_done(PrintObjectStep step) const
{
    if (this->objects.empty()) return false;
    FOREACH_OBJECT(this, object) {
        if (!(*object)->state.is_done(step))
            return false;
    }
    return true;
}

// returns 0-based indices of used extruders
std::set<size_t>
Print::object_extruders() const
{
    std::set<size_t> extruders;
    
    FOREACH_REGION(this, region) {
        // these checks reflect the same logic used in the GUI for enabling/disabling
        // extruder selection fields
        if ((*region)->config.perimeters.value > 0
            || this->config.brim_width.value > 0
            || this->config.interior_brim_width.value > 0
            || this->config.brim_connections_width.value > 0)
            extruders.insert((*region)->config.perimeter_extruder - 1);
        
        if ((*region)->config.fill_density.value > 0)
            extruders.insert((*region)->config.infill_extruder - 1);
        
        if ((*region)->config.top_solid_layers.value > 0 || (*region)->config.bottom_solid_layers.value > 0)
            extruders.insert((*region)->config.solid_infill_extruder - 1);
    }
    
    return extruders;
}

// returns 0-based indices of used extruders
std::set<size_t>
Print::support_material_extruders() const
{
    std::set<size_t> extruders;
    
    FOREACH_OBJECT(this, object) {
        if ((*object)->has_support_material()) {
            extruders.insert((*object)->config.support_material_extruder - 1);
            extruders.insert((*object)->config.support_material_interface_extruder - 1);
        }
    }
    
    return extruders;
}

// returns 0-based indices of used extruders
std::set<size_t>
Print::extruders() const
{
    std::set<size_t> extruders = this->object_extruders();
    
    std::set<size_t> s_extruders = this->support_material_extruders();
    extruders.insert(s_extruders.begin(), s_extruders.end());
    
    return extruders;
}

size_t
Print::brim_extruder() const
{
    size_t e = this->get_region(0)->config.perimeter_extruder;
    for (const PrintObject* object : this->objects) {
        if (object->config.raft_layers > 0)
            e = object->config.support_material_extruder;
    }
    return e;
}

void
Print::_simplify_slices(double distance)
{
    FOREACH_OBJECT(this, object) {
        FOREACH_LAYER(*object, layer) {
            (*layer)->slices.simplify(distance);
            FOREACH_LAYERREGION(*layer, layerm) {
                (*layerm)->slices.simplify(distance);
            }
        }
    }
}

double
Print::max_allowed_layer_height() const
{
    std::vector<double> nozzle_diameter;
    
    std::set<size_t> extruders = this->extruders();
    for (std::set<size_t>::const_iterator e = extruders.begin(); e != extruders.end(); ++e) {
        nozzle_diameter.push_back(this->config.nozzle_diameter.get_at(*e));
    }
    
    return *std::max_element(nozzle_diameter.begin(), nozzle_diameter.end());
}

//////////////////////////////////////////////////////////////
////BEGIN CODE TO IMPORT DIRECTLY SLICES FROM PATHFILES///////
//////////////////////////////////////////////////////////////

//handy type to read values from record headers
typedef union T64 {
    ClipperLib::cInt i;
    double d;
} T64;
typedef struct VoxelFileSpec {
    double xrad;
    double zrad;
    double zheight;
    double z_applicationPoint;
} VoxelFileSpec;
typedef struct ZSpec {
  double z;
  double height;
  bool useheight;
} ZSpec;
typedef std::vector<std::vector<ZSpec> > ZSpecsByTool;
//configuration option
enum InitialLayerMode {InitialLayerTool0, InitialLayerMinimalZ, InitialLayerAlways, InitialLayerNever};
//some shorthands
#define EARLY_EXIT {exit(-1);}
#define CONFESS_AND_EXIT(...) {CONFESS(__VA_ARGS__); EARLY_EXIT; }
#define CONFESS_AND_EXIT_VAL(val, ...) {CONFESS(__VA_ARGS__); return val; }
//definitions from multiresolution.h
#define PATHTYPE_RAW_CONTOUR        0
#define PATHTYPE_PROCESSED_CONTOUR  1
#define PATHTYPE_TOOLPATH_PERIMETER 2
#define PATHTYPE_TOOLPATH_INFILLING 3
#define PATHFORMAT_INT64     0
#define PATHFORMAT_DOUBLE    1
#define PATHFORMAT_DOUBLE_3D 2

//#define DEBUG_IMPORT_PATHS

//read all Z values of contours from pathsfile
bool getZValues(std::vector<bool> &usedTool, std::vector<VoxelFileSpec> &voxels, ZSpecsByTool &zss, double &minimal_z, std::string &file) {
    //open pathsfile
    FILE  *f = fopen(file.c_str(), "rb");
    ClipperLib::cInt numtools, useSched, numRecords;
    minimal_z = INFINITY;
    int version;
    
    fseek(f, 4, SEEK_CUR); //skip "PATH"
    if (fread(&version, sizeof(version), 1, f) != 1) CONFESS_AND_EXIT_VAL(false, "could not read version from file!");
    if (version > 1) CONFESS_AND_EXIT_VAL(false, "File has non-supported version!!!");
    
    //read file header
    if (fread(&numtools, sizeof(numtools), 1, f) != 1) CONFESS_AND_EXIT_VAL(false, "could not read numtools from file!");
    if (fread(&useSched, sizeof(useSched), 1, f) != 1) CONFESS_AND_EXIT_VAL(false, "could not read useSched from file!");
    
    usedTool.clear();
    usedTool.resize(numtools, false);
    voxels.clear();
    voxels.resize(numtools);
    for (int i=0; i<numtools; ++i) {
        if (fread(&voxels[i].xrad,                 sizeof(double), 1, f) != 1) CONFESS_AND_EXIT_VAL(false, "could not read xrad from file!");
        if (useSched) {
          if (fread(&voxels[i].zrad,               sizeof(double), 1, f) != 1) CONFESS_AND_EXIT_VAL(false, "could not read zrad from file!");
          if (fread(&voxels[i].zheight,            sizeof(double), 1, f) != 1) CONFESS_AND_EXIT_VAL(false, "could not read zheight from file!");
          if (fread(&voxels[i].z_applicationPoint, sizeof(double), 1, f) != 1) CONFESS_AND_EXIT_VAL(false, "could not read z_app_point from file!");
#ifdef DEBUG_IMPORT_PATHS
          printf("For tool %d: xrad=%f, zrad=%f, zheight=%f, appPoint=%f\n", i, voxels[i].xrad, voxels[i].zrad, voxels[i].zheight, voxels[i].z_applicationPoint);
#endif
        }
    }
    
    if (version > 0) {
        ClipperLib::cInt numadditionals;
        if (fread(&numadditionals, sizeof(numadditionals), 1, f) != 1) CONFESS_AND_EXIT_VAL(false, "could not read numadditionals from file!");
        if (numadditionals > 0) {
            fseek(f, numadditionals * sizeof(numadditionals), SEEK_CUR); //skip additionals
        }
    }

    if (fread(&numRecords, sizeof(numRecords), 1, f) != 1) CONFESS_AND_EXIT_VAL(false, "could not read numRecords from file!");
    
    zss.clear();
    zss.resize(numtools);
    std::vector<T64> alldata;

    for (int r=0; r<numRecords; ++r) {
      ClipperLib::cInt totalSize, headerSize, type, ntool;
      double z, height;
      
      if (fread(&totalSize,  sizeof(totalSize),  1, f) != 1) CONFESS_AND_EXIT_VAL(false, "could not read totalSize in record %d", r);
      if (fread(&headerSize, sizeof(headerSize), 1, f) != 1) CONFESS_AND_EXIT_VAL(false, "could not read headerSize in record %d", r);
      int numHR = headerSize/sizeof(T64);
      alldata.resize(numHR);
      if (alldata.size() < 2) CONFESS_AND_EXIT_VAL(false, "bad headerSize field (%d) in record %d", headerSize, r);
      int numToRead = (int)(headerSize - 2 * sizeof(T64));
      if (numHR<5) CONFESS_AND_EXIT_VAL(false, "slice header too short for record %d", r);
      if (fread(&(alldata[2]), 1, numToRead, f) != numToRead) CONFESS_AND_EXIT_VAL(false, "could not read slice header for record %d", r);
      ClipperLib::cInt numpaths;
      if (fread(&numpaths, sizeof(ClipperLib::cInt), 1, f) != 1) CONFESS_AND_EXIT_VAL(false, "could not read number of paths for record %d", r);
      if(fseek(f, (long)(totalSize - headerSize - sizeof(ClipperLib::cInt)), SEEK_CUR)!=0) CONFESS_AND_EXIT_VAL(false, "could not skip slice payload for record %d!", r);

      type       = alldata[2].i;
      ntool      = alldata[3].i;
      z          = alldata[4].d;
      
      if (type==PATHTYPE_PROCESSED_CONTOUR) {
        if ((ntool>=0) && (!usedTool[ntool])) usedTool[ntool] = numpaths>0;
        ZSpec spec;
        spec.z = z;
        spec.useheight = numHR>7;
        if (spec.useheight) {
          spec.height = alldata[7].d;
        }
        zss[ntool].push_back(spec);
      }
      if (z<minimal_z) {
        minimal_z = z;
      }
    }  
    fclose(f);
    return true;
}

//compute Z limits for each slice from the Z values of its neighbors (assumes that Z values are
bool getZLimits(int ntool, double z, std::vector<VoxelFileSpec> &voxels, ZSpecsByTool &zss, bool useSched, double &height, double &minZ, double &maxZ) {
  std::vector<ZSpec> &zs = zss[ntool];
  int maxk = zs.size()-1;
  //heuristics precedence: first additional header field, then Z params if useSched, then Z heights, then X radius
  for (int k=0; k< zs.size(); ++k) {
    if (zs[k].z==z) {
      if (zs[k].useheight) {
        height = zs[k].height;
        maxZ   = z+(height/2.0);
        minZ   = z-(height/2.0);
#ifdef DEBUG_IMPORT_PATHS
        printf("For z=%f, using ad hoc height: %f\n", z, height);
#endif
      } else if (useSched) {
        minZ   = z - voxels[ntool].z_applicationPoint;
        maxZ   = minZ + voxels[ntool].zheight;
        height = voxels[ntool].zheight;
#ifdef DEBUG_IMPORT_PATHS
        printf("For z=%f, using supplied height: %f\n", z, height);
#endif
      } else {
        int nextk = k+1;
        int prevk = k-1;
        while ( (nextk<=maxk) && (zs[nextk].z==z) ) {
          ++nextk;
        }
        if (prevk>=0) {
          minZ   = (z+zs[prevk].z)/2;
          if (nextk<=maxk) {
            maxZ   = (z+zs[nextk].z)/2;
          } else {
            maxZ   = z + (z-minZ);
          }
        } else {
          if (nextk<=maxk) {
            maxZ   = (z+zs[nextk].z)/2;
          } else {
            //in this case, we have only one slice. The only way we can solve this is by using radius data!
            maxZ = z + voxels[ntool].xrad;
          }
          minZ = z - (maxZ-z);
        }
        height = maxZ-minZ;
#ifdef DEBUG_IMPORT_PATHS
        printf("For z=%f, inferring height: %f\n", z, height);
#endif
      }
      return true;
    }
  }
  return false;
}

void
Print::add_print_from_slices(std::string slicesInputFile, const Pointf *center)
{
    if (sizeof(ClipperLib::cInt)!=sizeof(double)) CONFESS_AND_EXIT("method add_print_from_slices() would fail because sizeof(double)==%d but sizeof(ClipperLib::cInt)==%d!", sizeof(double), sizeof(ClipperLib::cInt));
    if (sizeof(int)!=4) CONFESS_AND_EXIT("method add_print_from_slices() would fail because sizeof(int)!=4!");
    
    //TODO: this should be configurable from command line, but, for now it is set at compile-time
    //InitialLayerMode initialLayerMode = InitialLayerTool0; //only the 0-th will have a non-empty layer with id=0
    //InitialLayerMode initialLayerMode = InitialLayerMinimalZ; //all tools with a layer at minimal Z will have non-empty layers with id=0
    //InitialLayerMode initialLayerMode = InitialLayerAlways; //all tools will have non-empty layers with id=0
    InitialLayerMode initialLayerMode = InitialLayerNever; //no tool will have a non-empty layer with id=0
    
    /*To compute Z boundaries between slices, we need to know the Z values of their immediate neighbours up and down.
     * While we can do this in one pass, it would make the code quite a bit more complex.
     * For now, let's do it in two passes. Hopefully, the first pass will be fast,
     * since we only read the record headers and use fseek() to skim over the record payloads*/
    ZSpecsByTool zss;
    std::vector<VoxelFileSpec> voxels;
    std::vector<bool> usedTool;
    double minimal_z;
    if (!getZValues(usedTool, voxels, zss, minimal_z, slicesInputFile)) EARLY_EXIT;
    
    //open pathsfile
    FILE  *f = fopen(slicesInputFile.c_str(), "rb");
    int version;
    ClipperLib::cInt numtools, useSched, numRecords;
    
    fseek(f, 4, SEEK_CUR); //skip "PATH"
    if (fread(&version, sizeof(version), 1, f) != 1) CONFESS_AND_EXIT("could not read version from file!");
    if (version > 1) CONFESS_AND_EXIT("File has non-supported version!!!");
    
    //read file header
    if (fread(&numtools, sizeof(numtools), 1, f) != 1) CONFESS_AND_EXIT("could not read numtools from file!");
    if (fread(&useSched, sizeof(useSched), 1, f) != 1) CONFESS_AND_EXIT("could not read useSched from file!");

    long numToSkip = sizeof(ClipperLib::cInt)*numtools;
    if (useSched) numToSkip *= 4;
    if (fseek(f, numToSkip, SEEK_CUR)!=0) CONFESS_AND_EXIT("could not skip radiuses in file header!");
    
    if (version > 0) {
        ClipperLib::cInt numadditionals;
        if (fread(&numadditionals, sizeof(numadditionals), 1, f) != 1) CONFESS_AND_EXIT("could not read numadditionals from file!");
        if (numadditionals > 0) {
            fseek(f, numadditionals * sizeof(numadditionals), SEEK_CUR); //skip additionals
        }
    }

    if (fread(&numRecords, sizeof(numRecords), 1, f) != 1) CONFESS_AND_EXIT("could not read numRecords from file!");
    
    std::vector<T64> alldata;
    
    std::vector<PrintRegion*> printregions(numtools);
    std::vector<PrintObject*> printobjects(numtools);
    std::vector<LayerRegion*> layerregions(numtools);
    for (int ntool=0; ntool<numtools; ++ntool) {
      printregions[ntool] = NULL;
      printobjects[ntool] = NULL;
      layerregions[ntool] = NULL;
    }
    std::vector<BoundingBox> bbs(numtools);
    std::vector<bool> firstTime(numtools, true);
    std::vector<double> minz(numtools);
    std::vector<double> maxz(numtools);
    std::vector<double> previousZ(numtools);
    std::vector<int> previousZRecord(numtools);
    std::vector<int> layerIds(numtools, 0);
    
    //Slic3r objects must be created in strict order, otherwise Slic3r gets the extruder indexes wrong
    for (int ntool=0; ntool<numtools; ++ntool) {
      if (usedTool[ntool]) {
          printobjects[ntool] = new PrintObject(this);
          printregions[ntool] = this->add_region();
      }
    }

    BoundingBox bball;
    ClipperLib::Paths paths;
    ExPolygons expolygons;
    Polygons polygons;
    std::vector<ClipperLib::DoublePoint> dpath;
    Point centeri(scale_(center->x), scale_(center->y));
    
    //read file
    for (int r=0; r<numRecords; ++r) {
      ClipperLib::cInt totalSize, headerSize, type, ntool, saveFormat, numpaths, numpoints;
      double z, scaling, adapt_fac, height, minZ, maxZ;
      int lc;

      if (fread(&totalSize,  sizeof(totalSize),  1, f) != 1) CONFESS_AND_EXIT("could not read totalSize in record %d", r);
      if (fread(&headerSize, sizeof(headerSize), 1, f) != 1) CONFESS_AND_EXIT("could not read headerSize in record %d", r);
      alldata.resize(headerSize/sizeof(T64));
      if (alldata.size() < 2) CONFESS_AND_EXIT("bad headerSize field (%d) in record %d", headerSize, r);
      ClipperLib::cInt numToRead = (int)(headerSize - 2 * sizeof(T64));
      if (numToRead<5) CONFESS_AND_EXIT("slice header too short for record %d", r);
      if (fread(&(alldata[2]), 1, numToRead, f) != numToRead) CONFESS_AND_EXIT("could not read slice header for record %d", r);
      type       = alldata[2].i;
      ntool      = alldata[3].i;
      z          = alldata[4].d;
      saveFormat = alldata[5].i;
      scaling    = alldata[6].d;
      adapt_fac  = scaling/SCALING_FACTOR;

      if (ntool>=numtools) CONFESS_AND_EXIT("file had numtools=%d, but ntool=%d for record %d!", numtools, ntool, r);
      
      //z is not in scaled units (as opposed to the XY coordinates of the actual paths)
      
      //2D paths in double format should be readable with a little effort, but there will be no easy solution for 3D paths
      if ((saveFormat!=PATHFORMAT_INT64) && (saveFormat!=PATHFORMAT_DOUBLE)) CONFESS_AND_EXIT("contour in record %d/%d is not in a valid format. numToRead=%lld, type=%lld, ntool=%lld, z=%f, saveFormat=%lld, scaling=%f", r, numRecords, ((long long int)alldata.size())-2, type, ntool, z, saveFormat, scaling);
      
      bool isInt64 = saveFormat==PATHFORMAT_INT64;
      
      if (ntool>=numtools) CONFESS_AND_EXIT("file had numtools=%lld, but ntool=%lld for record %lld!", numtools, ntool, r);
      if (ntool<-1) CONFESS_AND_EXIT("record %d had invalid ntool %lld!", r, ntool);
      
      if (type!=PATHTYPE_PROCESSED_CONTOUR) {
          if (fseek(f, (long)(totalSize - headerSize), SEEK_CUR)!=0) CONFESS_AND_EXIT("could not seek past payload for record %d", r);
          continue;
      }
      
      //do Z value book-keeping and setup Slic3r object hierarchy
      BoundingBox &bb      = bbs[ntool];
      bool sameZAsPrevious = false;
      if (firstTime[ntool]) {
          //prepare object without proper bb
          minz[ntool] =  INFINITY;
          maxz[ntool] = -INFINITY;
      } else {
          if (previousZ[ntool]>z) CONFESS_AND_EXIT("Slic3r requires the slices for each tool to be ordered by monotonically increasing Z value, but this was not true for tool %d: record %d had z=%f, while record %d has z=%f", ntool, previousZRecord[ntool], previousZ[ntool], r, z);
          sameZAsPrevious = previousZ[ntool]==z;
      }
      
      //read record payload
      if (fread(&numpaths, sizeof(ClipperLib::cInt), 1, f)!=1)  CONFESS_AND_EXIT("could not read path file, record %d!", r);
      paths.clear();
      paths.resize(numpaths);
      for (ClipperLib::Paths::iterator pp = paths.begin(); pp != paths.end(); ++pp) {
          //read numpoints
          if (fread(&numpoints, sizeof(ClipperLib::cInt), 1, f)!=1) CONFESS_AND_EXIT("could not read path file, record %d, path %d!", r, pp-paths.begin());
          pp->clear();
          pp->resize(numpoints);
      }
      for (ClipperLib::Paths::iterator pp = paths.begin(); pp != paths.end(); ++pp) {
          //read points
          std::vector<ClipperLib::DoublePoint>::iterator dit;
          if (isInt64) {
            if (fread(&(pp->front()), sizeof(ClipperLib::cInt)*2, pp->size(), f)!=pp->size()) CONFESS_AND_EXIT("could not read path file, record %d, path %d!", r, pp-paths.begin());
          } else {
            dpath.resize(pp->size());
            if (fread(&(dpath.front()), sizeof(double)*2, pp->size(), f)!=pp->size()) CONFESS_AND_EXIT("could not read path file, record %d, path %d!", r, pp-paths.begin());
            dit = dpath.begin();
          }
          for (ClipperLib::Path::iterator p = pp->begin(); p != pp->end(); ++p) {
              if (isInt64) {
                p->X *= adapt_fac;
                p->Y *= adapt_fac;
              } else {
                p->X = dit->X/SCALING_FACTOR;
                p->Y = dit->Y/SCALING_FACTOR;
                ++dit;
              }
              
              if (bb.defined) {
                if (p->X<bb.min.x) bb.min.x = p->X;
                if (p->X>bb.max.x) bb.max.x = p->X;
                if (p->Y<bb.min.y) bb.min.y = p->Y;
                if (p->Y>bb.max.y) bb.max.y = p->Y;
              } else {
                bb.min.x = p->X;
                bb.max.x = p->X;
                bb.min.y = p->Y;
                bb.max.y = p->Y;
                bb.defined = true;
              }
              if (bball.defined) {
                if (p->X<bball.min.x) bball.min.x = p->X;
                if (p->X>bball.max.x) bball.max.x = p->X;
                if (p->Y<bball.min.y) bball.min.y = p->Y;
                if (p->Y>bball.max.y) bball.max.y = p->Y;
              } else {
                bball.min.x = p->X;
                bball.max.x = p->X;
                bball.min.y = p->Y;
                bball.max.y = p->Y;
                bball.defined = true;
              }
          }
          if (!isInt64) dpath.clear();
      }
      
      //convert to Slic3r data format
      expolygons = ClipperPaths_to_Slic3rExPolygons(paths);
      paths.clear();

      if (!expolygons.empty()) {
        //simplify polygons (crucial to shorten computing times for the rest of the pipeline, given that pathsfiles usually contain slices at very big resolution)
        for (ExPolygons::iterator exp = expolygons.begin(); exp != expolygons.end(); ++exp) {
            //TODO: tolerance should be configurable, but for now it is hardcoded
            exp->simplify_p(SCALED_RESOLUTION/2, &polygons);
        }
        expolygons = union_ex(polygons);
        polygons.clear();
      }

#ifdef DEBUG_IMPORT_PATHS
      BoundingBox bbAll1;
      if (!expolygons.empty()) {
        bbAll1 = expolygons.begin()->contour.bounding_box();
        for (ExPolygons::iterator exp = expolygons.begin()+1; exp != expolygons.end(); ++exp) {
            bbAll1.merge(exp->contour.bounding_box());
        }
      }
#endif
      
      firstTime[ntool] = false;
      //set these only after making sure that the effective contours are not empty
      previousZ[ntool]        = z;
      previousZRecord[ntool]  = r;
      
      //do Z limits book-keeping and setup Slic3r data holder object
      LayerRegion* currentlayerregion;
      if (!sameZAsPrevious) {
          if (!getZLimits(ntool, z, voxels, zss, useSched, height, minZ, maxZ)) CONFESS_AND_EXIT("Could not find z=%f for record %d", z, r);
          
          if (minZ<minz[ntool]) minz[ntool] = minZ;
          if (maxZ>maxz[ntool]) maxz[ntool] = maxZ;
          
          PrintObject *obj = printobjects[ntool];
          
          //layers with id=0 must be handled with care, because they trigger lots of alternative code paths
          if (layerIds[ntool]==0) {
            bool addEmptyInitialLayer;
            switch (initialLayerMode) {
              case InitialLayerTool0:          addEmptyInitialLayer = ntool!=0;     break; //in this case, we only have a non-empty initial layer for the tool 0
              case InitialLayerMinimalZ:       addEmptyInitialLayer = z!=minimal_z; break; //in this case, we only have a non-empty initial layer for layers at minimal Z
              case InitialLayerAlways:         addEmptyInitialLayer = true;         break;
              case InitialLayerNever: default: addEmptyInitialLayer = false;
            };
            if (addEmptyInitialLayer) {
#ifdef DEBUG_IMPORT_PATHS
              printf("ADDING EMPTY LAYER: record %d, ntool=%d, id=%d, height=%g, maxZ=%g, z=%g\n", r, ntool, layerIds[ntool], height, maxZ-height, minZ-height/2);
#endif
              obj->add_layer(layerIds[ntool]++, height, maxZ-height, minZ-height/2);
            }
          }
#ifdef DEBUG_IMPORT_PATHS
          printf("ADDING LAYER: record %d, ntool=%d, id=%d, height=%g, maxZ=%g, z=%g\n", r, ntool, layerIds[ntool], height, maxZ, z);
          if (!expolygons.empty()) {
            printf("    ->BOUNDINGBOX INITIAL: XMIN=%lld, XMAX=%lld, YMIN=%lld, YMAX=%lld\n", bbAll1.min.x, bbAll1.max.x, bbAll1.min.y, bbAll1.max.y);
            printf("    ->BOUNDINGBOX UNSCALE: XMIN=%g, XMAX=%g, YMIN=%g, YMAX=%g\n", unscale(bbAll1.min.x), unscale(bbAll1.max.x), unscale(bbAll1.min.y), unscale(bbAll1.max.y));
          }
#endif
          Layer * layer = obj->add_layer(layerIds[ntool]++, height, maxZ, minZ+height/2);
          int lc = obj->layer_count();
          if (lc>=2) {
              obj->get_layer(lc-2)->upper_layer = obj->get_layer(lc-1);
              obj->get_layer(lc-1)->lower_layer = obj->get_layer(lc-2);
          }
          /*Extruders are indexed by LayerRegions. Each layer region has an id depending on its inserting order in the Layer
            BUT EACH LAYER expects to have a LayerRegion for each PrintRegion.
            So, add non-used LayerRegions so the LayerRegion for this Layer has the approperiate id!*/
          for (int nt=0; nt<numtools; ++nt) {
            //the algorithm in Layer::make_perimeters() requires to use all printRegions in each object, otherwise the perimeters end up assigned to the wrong extruder
            if (usedTool[nt]) {
              LayerRegion* tmp = layer->add_region(printregions[nt]);
              if (nt==ntool) {
                currentlayerregion = layerregions[ntool] = tmp;
              }
            }
          }
      } else {
#ifdef DEBUG_IMPORT_PATHS
              printf("ADDING SLICE TO EXISTING LAYER: record %d, ntool=%d, id=%d, height=%g, maxZ=%g, z=%g\n", r, ntool, layerIds[ntool], height, maxZ, z);
#endif
          currentlayerregion = layerregions[ntool];
      }
      
      //transfer data to Slic3r data holder object
      for (ExPolygons::iterator exp = expolygons.begin(); exp != expolygons.end(); ++exp) {
          currentlayerregion->slices.surfaces.push_back(Surface(stInternal, *exp));
      }
      expolygons.clear();
    }
    fclose(f);
    
    Size sizeall = bball.size();
    coord_t shift_x = -bball.min.x + centeri.x - sizeall.x/2;
    coord_t shift_y = -bball.min.y + centeri.y - sizeall.y/2;
#ifdef DEBUG_IMPORT_PATHS
    printf("FOR CENTER:\n");
    printf("   x: %g (in int64: %lld), SHIFT in int64: %lld\n", center->x, centeri.x, shift_x);
    printf("   y: %g (in int64: %lld), SHIFT in int64: %lld\n", center->y, centeri.y, shift_y);
#endif
    //apply BoundingBoxes and extruder configurations to Slic3r objects
    for (int ntool=0; ntool<numtools; ++ntool) {
        if (!usedTool[ntool]) continue;
        BoundingBox &bb  = bbs[ntool];
        BoundingBoxf3 bbf3;
        Size size;
        if (bb.defined) { //if !defined, all coordinates/sizes are 0
          bbf3.min.x     = unscale(bb.min.x);
          bbf3.min.y     = unscale(bb.min.y);
          bbf3.max.z     = maxz[ntool];
          bbf3.max.x     = unscale(bb.max.x);
          bbf3.max.y     = unscale(bb.max.y);
          bbf3.min.z     = minz[ntool];
          size           = bb.size();
        }
        
        PrintObject *obj = printobjects[ntool];
        PrintRegion *reg = printregions[ntool];
        obj->set_modobj_bbox(bbf3, false);
#ifdef DEBUG_IMPORT_PATHS
        printf("FINAL BOUNDING BOX FOR ntool=%d\n", ntool);
        printf("   xmax:%g\n", bbf3.max.x);
        printf("   xmin:%g\n", bbf3.min.x);
        printf("   xsiz:%g\n", bbf3.max.x-bbf3.min.x);
        printf("   ymax:%g\n", bbf3.max.y);
        printf("   ymax:%g\n", bbf3.min.y);
        printf("   ysiz:%g\n", bbf3.max.y-bbf3.min.y);
        printf("   ymax:%g\n", bbf3.max.z);
        printf("   ymax:%g\n", bbf3.min.z);
        printf("   ysiz:%g\n", bbf3.max.z-bbf3.min.z);
        printf("FINAL int64 size FOR ntool=%d\n", ntool);
        printf("   x:%lld\n", obj->size.x);
        printf("   y:%lld\n", obj->size.y);
        printf("   z:%lld\n", obj->size.z);
#endif
        Points copy(1, Point(shift_x, shift_y));
        obj->set_copies(copy);
        reg->config.apply(this->default_region_config);
        // initialize and add new print object
        objects.push_back(obj);
        // apply config to print object
        obj->config.apply(this->default_object_config);
        
        if (numtools>1) {
            size_t extruder_id = ntool+1;
            //we have to generate a new DynamicPrintConfig object for each extruder value
            //because of the way DynamicPrintConfig::normalize() works
            DynamicPrintConfig dynamicConfig;
            dynamicConfig.opt<ConfigOptionInt>("extruder", true)->value = extruder_id;
            dynamicConfig.normalize();
            reg->config.apply(dynamicConfig, true);
            obj->config.apply(dynamicConfig, true);
        }
    }
#ifdef DEBUG_IMPORT_PATHS
    fflush(stdout);
#endif
}

//////////////////////////////////////////////////////////////
////END CODE TO IMPORT DIRECTLY SLICES FROM PATHFILES/////////
//////////////////////////////////////////////////////////////

/*  Caller is responsible for supplying models whose objects don't collide
    and have explicit instance positions */
void
Print::add_model_object(ModelObject* model_object, int idx)
{
    DynamicPrintConfig object_config = model_object->config;  // clone
    object_config.normalize();

    // initialize print object and store it at the given position
    PrintObject* o;
    {
        BoundingBoxf3 bb = model_object->raw_bounding_box();
        if (idx != -1) {
            // replacing existing object
            PrintObjectPtrs::iterator old_it = this->objects.begin() + idx;
            // before deleting object, invalidate all of its steps in order to 
            // invalidate all of the dependent ones in Print
            (*old_it)->invalidate_all_steps();
            delete *old_it;
            
            this->objects[idx] = o = new PrintObject(this, model_object, bb);
        } else {
            o = new PrintObject(this, model_object, bb);
            objects.push_back(o);
    
            // invalidate steps
            this->invalidate_step(psSkirt);
            this->invalidate_step(psBrim);
        }
    }

    for (ModelVolumePtrs::const_iterator v_i = model_object->volumes.begin(); v_i != model_object->volumes.end(); ++v_i) {
        size_t volume_id = v_i - model_object->volumes.begin();
        ModelVolume* volume = *v_i;
        
        // get the config applied to this volume
        PrintRegionConfig config = this->_region_config_from_model_volume(*volume);
        
        // find an existing print region with the same config
        int region_id = -1;
        for (PrintRegionPtrs::const_iterator region = this->regions.begin(); region != this->regions.end(); ++region) {
            if (config.equals((*region)->config)) {
                region_id = region - this->regions.begin();
                break;
            }
        }
        
        // if no region exists with the same config, create a new one
        if (region_id == -1) {
            PrintRegion* r = this->add_region();
            r->config.apply(config);
            region_id = this->regions.size() - 1;
        }
        
        // assign volume to region
        o->add_region_volume(region_id, volume_id);
    }

    // apply config to print object
    o->config.apply(this->default_object_config);
    o->config.apply(object_config, true);
    
    // update placeholders
    {
        // get the first input file name
        std::string input_file;
        std::vector<std::string> v_scale;
        FOREACH_OBJECT(this, object) {
            const ModelObject &mobj = *(*object)->model_object();
            v_scale.push_back( boost::lexical_cast<std::string>(mobj.instances[0]->scaling_factor*100) + "%" );
            if (input_file.empty())
                input_file = mobj.input_file;
        }
        
        PlaceholderParser &pp = this->placeholder_parser;
        pp.set("scale", v_scale);
        if (!input_file.empty()) {
            // get basename with and without suffix
            const std::string input_basename = boost::filesystem::path(input_file).filename().string();
            pp.set("input_filename", input_basename);
            const std::string input_basename_base = input_basename.substr(0, input_basename.find_last_of("."));
            pp.set("input_filename_base", input_basename_base);
        }
    }
}

bool
Print::apply_config(DynamicPrintConfig config)
{
    // we get a copy of the config object so we can modify it safely
    config.normalize();
    
    // apply variables to placeholder parser
    this->placeholder_parser.apply_config(config);
    
    // handle changes to print config
    bool invalidated = this->invalidate_state_by_config(config);
    
    // handle changes to object config defaults
    this->default_object_config.apply(config, true);
    for (PrintObject* object : this->objects) {
        // we don't assume that config contains a full ObjectConfig,
        // so we base it on the current print-wise default
        PrintObjectConfig new_config = this->default_object_config;
        new_config.apply(config, true);
        
        // we override the new config with object-specific options
        if (object->model_object()!=NULL) {
            DynamicPrintConfig model_object_config = object->model_object()->config;
            model_object_config.normalize();
            new_config.apply(model_object_config, true);
        }
        
        // check whether the new config is different from the current one
        if (object->invalidate_state_by_config(new_config))
            invalidated = true;
    }
    
    // handle changes to regions config defaults
    this->default_region_config.apply(config, true);
    
    // All regions now have distinct settings.
    // Check whether applying the new region config defaults we'd get different regions.
    bool rearrange_regions = false;
    std::vector<PrintRegionConfig> other_region_configs;
    FOREACH_REGION(this, it_r) {
        size_t region_id = it_r - this->regions.begin();
        PrintRegion* region = *it_r;
        
        std::vector<PrintRegionConfig> this_region_configs;
        FOREACH_OBJECT(this, it_o) {
            PrintObject* object = *it_o;
            
            if (object->model_object()==NULL) continue;
            std::vector<int> &region_volumes = object->region_volumes[region_id];
            for (std::vector<int>::const_iterator volume_id = region_volumes.begin(); volume_id != region_volumes.end(); ++volume_id) {
                ModelVolume* volume = object->model_object()->volumes.at(*volume_id);
                
                PrintRegionConfig new_config = this->_region_config_from_model_volume(*volume);
                
                for (std::vector<PrintRegionConfig>::iterator it = this_region_configs.begin(); it != this_region_configs.end(); ++it) {
                    // if the new config for this volume differs from the other
                    // volume configs currently associated to this region, it means
                    // the region subdivision does not make sense anymore
                    if (!it->equals(new_config)) {
                        rearrange_regions = true;
                        goto NEXT_REGION;
                    }
                }
                this_region_configs.push_back(new_config);
                
                for (std::vector<PrintRegionConfig>::iterator it = other_region_configs.begin(); it != other_region_configs.end(); ++it) {
                    // if the new config for this volume equals any of the other
                    // volume configs that are not currently associated to this
                    // region, it means the region subdivision does not make
                    // sense anymore
                    if (it->equals(new_config)) {
                        rearrange_regions = true;
                        goto NEXT_REGION;
                    }
                }
                
                // if we're here and the new region config is different from the old
                // one, we need to apply the new config and invalidate all objects
                // (possible optimization: only invalidate objects using this region)
                if (region->invalidate_state_by_config(new_config))
                    invalidated = true;
            }
        }
        other_region_configs.insert(other_region_configs.end(), this_region_configs.begin(), this_region_configs.end());
        
        NEXT_REGION:
            continue;
    }
    
    if (rearrange_regions) {
        // the current subdivision of regions does not make sense anymore.
        // we need to remove all objects and re-add them
        ModelObjectPtrs model_objects;
        FOREACH_OBJECT(this, o) {
            model_objects.push_back((*o)->model_object());
        }
        this->clear_objects();
        for (ModelObjectPtrs::iterator it = model_objects.begin(); it != model_objects.end(); ++it) {
            this->add_model_object(*it);
        }
        invalidated = true;
    }
    
    return invalidated;
}

bool Print::has_infinite_skirt() const
{
    return (this->config.skirt_height == -1 && this->config.skirts > 0)
        || (this->config.ooze_prevention && this->extruders().size() > 1);
}

bool Print::has_skirt() const
{
    return (this->config.skirt_height > 0 && this->config.skirts > 0)
        || this->has_infinite_skirt();
}

std::string
Print::validate() const
{
    if (this->config.complete_objects) {
        // check horizontal clearance
        {
            Polygons a;
            FOREACH_OBJECT(this, i_object) {
                PrintObject* object = *i_object;
                
                if (object->model_object()==NULL) continue;
                
                /*  get convex hull of all meshes assigned to this print object
                    (this is the same as model_object()->raw_mesh.convex_hull()
                    but probably more efficient */
                Polygon convex_hull;
                {
                    Polygons mesh_convex_hulls;
                    for (size_t i = 0; i < this->regions.size(); ++i) {
                        for (std::vector<int>::const_iterator it = object->region_volumes[i].begin(); it != object->region_volumes[i].end(); ++it) {
                            Polygon hull = object->model_object()->volumes[*it]->mesh.convex_hull();
                            mesh_convex_hulls.push_back(hull);
                        }
                    }
                
                    // make a single convex hull for all of them
                    convex_hull = Slic3r::Geometry::convex_hull(mesh_convex_hulls);
                }
                
                // apply the same transformations we apply to the actual meshes when slicing them
                object->model_object()->instances.front()->transform_polygon(&convex_hull);
                
                // grow convex hull with the clearance margin
                convex_hull = offset(convex_hull, scale_(this->config.extruder_clearance_radius.value)/2, 1, jtRound, scale_(0.1)).front();
                
                // now we check that no instance of convex_hull intersects any of the previously checked object instances
                for (Points::const_iterator copy = object->_shifted_copies.begin(); copy != object->_shifted_copies.end(); ++copy) {
                    Polygon p = convex_hull;
                    p.translate(*copy);
                    if (!intersection(a, p).empty())
                        return "Some objects are too close; your extruder will collide with them.";
                    
                    a = union_(a, p);
                }
            }
        }
        
        // check vertical clearance
        {
            std::vector<coord_t> object_height;
            FOREACH_OBJECT(this, i_object) {
                PrintObject* object = *i_object;
                object_height.insert(object_height.end(), object->copies().size(), object->size.z);
            }
            std::sort(object_height.begin(), object_height.end());
            // ignore the tallest *copy* (this is why we repeat height for all of them):
            // it will be printed as last one so its height doesn't matter
            object_height.pop_back();
            if (!object_height.empty() && object_height.back() > scale_(this->config.extruder_clearance_height.value))
                return "Some objects are too tall and cannot be printed without extruder collisions.";
        }
    } // end if (this->config.complete_objects)
    
    if (this->config.spiral_vase) {
        size_t total_copies_count = 0;
        FOREACH_OBJECT(this, i_object) total_copies_count += (*i_object)->copies().size();
        if (total_copies_count > 1 && !this->config.complete_objects.getBool())
            return "The Spiral Vase option can only be used when printing a single object.";
        if (this->regions.size() > 1)
            return "The Spiral Vase option can only be used when printing single material objects.";
    }
    
    if (this->extruders().empty())
        return "The supplied settings will cause an empty print.";
    
    return std::string();
}

// the bounding box of objects placed in copies position
// (without taking skirt/brim/support material into account)
BoundingBox
Print::bounding_box() const
{
    BoundingBox bb;
    FOREACH_OBJECT(this, object) {
        for (Points::const_iterator copy = (*object)->_shifted_copies.begin(); copy != (*object)->_shifted_copies.end(); ++copy) {
            bb.merge(*copy);
            
            Point p = *copy;
            p.translate((*object)->size);
            bb.merge(p);
        }
    }
    return bb;
}

// the total bounding box of extrusions, including skirt/brim/support material
// this methods needs to be called even when no steps were processed, so it should
// only use configuration values
BoundingBox
Print::total_bounding_box() const
{
    // get objects bounding box
    BoundingBox bb = this->bounding_box();
    
    // we need to offset the objects bounding box by at least half the perimeters extrusion width
    Flow perimeter_flow = this->objects.front()->get_layer(0)->get_region(0)->flow(frPerimeter);
    double extra = perimeter_flow.width/2;
    
    // consider support material
    if (this->has_support_material()) {
        extra = std::max(extra, SUPPORT_MATERIAL_MARGIN);
    }
    
    // consider brim and skirt
    if (this->config.brim_width.value > 0) {
        Flow brim_flow = this->brim_flow();
        extra = std::max(extra, this->config.brim_width.value + brim_flow.width/2);
    }
    if (this->has_skirt()) {
        int skirts = this->config.skirts.value;
        if (skirts == 0 && this->has_infinite_skirt()) skirts = 1;
        Flow skirt_flow = this->skirt_flow();
        extra = std::max(
            extra,
            this->config.brim_width.value
                + this->config.skirt_distance.value
                + skirts * skirt_flow.spacing()
                + skirt_flow.width/2
        );
    }
    
    if (extra > 0)
        bb.offset(scale_(extra));
    
    return bb;
}

double
Print::skirt_first_layer_height() const
{
    if (this->objects.empty()) CONFESS("skirt_first_layer_height() can't be called without PrintObjects");
    return this->objects.front()->config.get_abs_value("first_layer_height");
}

// This will throw an exception when called without PrintObjects
Flow
Print::brim_flow() const
{
    ConfigOptionFloatOrPercent width = this->config.first_layer_extrusion_width;
    if (width.value == 0) width = this->regions.front()->config.perimeter_extrusion_width;
    
    /* We currently use a random region's perimeter extruder.
       While this works for most cases, we should probably consider all of the perimeter
       extruders and take the one with, say, the smallest index.
       The same logic should be applied to the code that selects the extruder during G-code
       generation as well. */
    Flow flow = Flow::new_from_config_width(
        frPerimeter,
        width, 
        this->config.nozzle_diameter.get_at(this->regions.front()->config.perimeter_extruder-1),
        this->skirt_first_layer_height(),
        0
    );
    
    // Adjust extrusion width in order to fill the total brim width with an integer number of lines.
    flow.set_solid_spacing(this->config.brim_width.value);
    
    return flow;
}

// This will throw an exception when called without PrintObjects
Flow
Print::skirt_flow() const
{
    ConfigOptionFloatOrPercent width = this->config.first_layer_extrusion_width;
    if (width.value == 0) width = this->regions.front()->config.perimeter_extrusion_width;
    
    /* We currently use a random object's support material extruder.
       While this works for most cases, we should probably consider all of the support material
       extruders and take the one with, say, the smallest index;
       The same logic should be applied to the code that selects the extruder during G-code
       generation as well. */
    return Flow::new_from_config_width(
        frPerimeter,
        width, 
        this->config.nozzle_diameter.get_at(this->objects.front()->config.support_material_extruder-1),
        this->skirt_first_layer_height(),
        0
    );
}

void
Print::_make_brim()
{
    if (this->state.is_done(psBrim)) return;
    this->state.set_started(psBrim);
    
    // since this method must be idempotent, we clear brim paths *before*
    // checking whether we need to generate them
    this->brim.clear();
    
    if (this->objects.empty()
        || (this->config.brim_width == 0
            && this->config.interior_brim_width == 0
            && this->config.brim_connections_width == 0)) {
        this->state.set_done(psBrim);
        return;
    }
    
    // brim is only printed on first layer and uses perimeter extruder
    const Flow flow  = this->brim_flow();
    const double mm3_per_mm = flow.mm3_per_mm();
    
    const coord_t grow_distance = flow.scaled_width()/2;
    Polygons islands;
    
    for (PrintObject* object : this->objects) {
        const Layer* layer0 = object->get_layer(0);
        
        Polygons object_islands = layer0->slices.contours();
        
        if (!object->support_layers.empty()) {
            const SupportLayer* support_layer0 = object->get_support_layer(0);
            
            for (const ExtrusionEntity* e : support_layer0->support_fills.entities)
                append_to(object_islands, offset(e->as_polyline(), grow_distance));
            
            for (const ExtrusionEntity* e : support_layer0->support_interface_fills.entities)
                append_to(object_islands, offset(e->as_polyline(), grow_distance));
        }
        for (const Point &copy : object->_shifted_copies) {
            for (Polygon p : object_islands) {
                p.translate(copy);
                islands.push_back(p);
            }
        }
    }
    
    Polygons loops;
    const int num_loops = floor(this->config.brim_width / flow.width + 0.5);
    for (int i = num_loops; i >= 1; --i) {
        // JT_SQUARE ensures no vertex is outside the given offset distance
        // -0.5 because islands are not represented by their centerlines
        // (first offset more, then step back - reverse order than the one used for 
        // perimeters because here we're offsetting outwards)
        append_to(loops, offset2(
            islands,
            flow.scaled_width() + flow.scaled_spacing() * (i - 1.5 + 0.5),
            flow.scaled_spacing() * -0.5,
            100000,
            ClipperLib::jtSquare
        ));
    }
    
    {
        Polygons chained = union_pt_chained(loops);
        for (Polygons::const_reverse_iterator p = chained.rbegin(); p != chained.rend(); ++p) {
            ExtrusionPath path(erSkirt, mm3_per_mm, flow.width, flow.height);
            path.polyline = p->split_at_first_point();
            this->brim.append(ExtrusionLoop(path));
        }
    }
    
    if (this->config.brim_connections_width > 0) {
        // get islands to connect
        for (Polygon &p : islands)
            p = Geometry::convex_hull(p.points);
        
        islands = offset(islands, flow.scaled_spacing() * (num_loops-0.2), 10000, jtSquare);
        
        // compute centroid for each island
        Points centroids;
        centroids.reserve(islands.size());
        for (const Polygon &p : islands)
            centroids.push_back(p.centroid());
        
        // in order to check visibility we need to account for the connections width,
        // so let's use grown islands
        const double scaled_width = scale_(this->config.brim_connections_width);
        const Polygons grown = offset(islands, +scaled_width/2);
        
        // find pairs of islands having direct visibility
        Lines lines;
        for (size_t i = 0; i < islands.size(); ++i) {
            for (size_t j = (i+1); j < islands.size(); ++j) {
                // check visibility
                Line line(centroids[i], centroids[j]);
                if (diff_pl((Polyline)line, grown).size() != 1) continue;
                lines.push_back(line);
            }
        }
        
        std::unique_ptr<Fill> filler(Fill::new_from_type(ipRectilinear));
        filler->min_spacing  = flow.spacing();
        filler->dont_adjust  = true;
        filler->density      = 1;
        
        // subtract already generated connections in order to prevent crossings
        // and overextrusion
        Polygons other;
        
        for (Lines::const_iterator line = lines.begin(); line != lines.end(); ++line) {
            ExPolygons expp = diff_ex(
                offset((Polyline)*line, scaled_width/2),
                islands + other
            );
            
            filler->angle = line->direction();
            for (ExPolygons::const_iterator ex = expp.begin(); ex != expp.end(); ++ex) {
                append_to(other, (Polygons)*ex);
                
                const Polylines paths = filler->fill_surface(Surface(stBottom, *ex));
                for (Polylines::const_iterator pl = paths.begin(); pl != paths.end(); ++pl) {
                    ExtrusionPath path(erSkirt, mm3_per_mm, flow.width, flow.height);
                    path.polyline = *pl;
                    this->brim.append(path);
                }
            }
        }
    }
    
    if (this->config.interior_brim_width > 0) {
        // collect all island holes to fill
        Polygons holes;
        for (const PrintObject* object : this->objects) {
            const Layer &layer0 = *object->get_layer(0);
            
            Polygons o_holes = layer0.slices.holes();
            
            // When we have no infill on this layer, consider the internal part
            // of the model as a hole.
            for (const LayerRegion* layerm : layer0.regions) {
                if (layerm->fills.empty())
                    append_to(o_holes, (Polygons)layerm->fill_surfaces);
            }
            
            for (const Point &copy : object->_shifted_copies) {
                for (Polygon p : o_holes) {
                    p.translate(copy);
                    holes.push_back(p);
                }
            }
        }
        
        Polygons loops;
        const int num_loops = floor(this->config.interior_brim_width / flow.width + 0.5);
        for (int i = 1; i <= num_loops; ++i) {
            append_to(loops, offset2(
                holes,
                -flow.scaled_spacing() * (i + 0.5),
                flow.scaled_spacing()
            ));
        }
        
        loops = union_pt_chained(loops);
        for (const Polygon &p : loops) {
            ExtrusionPath path(erSkirt, mm3_per_mm, flow.width, flow.height);
            path.polyline = p.split_at_first_point();
            this->brim.append(ExtrusionLoop(path));
        }
    }
    
    this->state.set_done(psBrim);
}


PrintRegionConfig
Print::_region_config_from_model_volume(const ModelVolume &volume)
{
    PrintRegionConfig config = this->default_region_config;
    {
        DynamicPrintConfig other_config = volume.get_object()->config;
        other_config.normalize();
        config.apply(other_config, true);
    }
    {
        DynamicPrintConfig other_config = volume.config;
        other_config.normalize();
        config.apply(other_config, true);
    }
    if (!volume.material_id().empty()) {
        DynamicPrintConfig material_config = volume.material()->config;
        material_config.normalize();
        config.apply(material_config, true);
    }
    return config;
}

bool
Print::has_support_material() const
{
    FOREACH_OBJECT(this, object) {
        if ((*object)->has_support_material()) return true;
    }
    return false;
}

/*  This method assigns extruders to the volumes having a material
    but not having extruders set in the volume config. */
void
Print::auto_assign_extruders(ModelObject* model_object) const
{
    // only assign extruders if object has more than one volume
    if (model_object->volumes.size() < 2) return;
    
    for (ModelVolumePtrs::const_iterator v = model_object->volumes.begin(); v != model_object->volumes.end(); ++v) {
        if (!(*v)->material_id().empty()) {
            //FIXME Vojtech: This assigns an extruder ID even to a modifier volume, if it has a material assigned.
            size_t extruder_id = (v - model_object->volumes.begin()) + 1;
            if (!(*v)->config.has("extruder"))
                (*v)->config.opt<ConfigOptionInt>("extruder", true)->value = extruder_id;
        }
    }
}

std::string
Print::output_filename()
{
    this->placeholder_parser.update_timestamp();
    return this->placeholder_parser.process(this->config.output_filename_format.value);
}

std::string
Print::output_filepath(const std::string &path)
{
    // if we were supplied no path, generate an automatic one based on our first object's input file
    if (path.empty()) {
        // get the first input file name
        std::string input_file;
        FOREACH_OBJECT(this, object) {
            if ((*object)->model_object()==NULL) continue;
            input_file = (*object)->model_object()->input_file;
            if (!input_file.empty()) break;
        }
        if (input_file.empty() && useOutputPath()) {
            input_file = getOutputPath();
            //Of these 3 values added to the placeholder, I am pretty sure that the last two are necessary.
            //The first one (scale) I am not sure if it is required, but I am putting it anyway.
            PlaceholderParser &pp = this->placeholder_parser;
            std::vector<std::string> v_scale;
            FOREACH_OBJECT(this, object) {
                v_scale.push_back( boost::lexical_cast<std::string>(1.0*100) + "%" );
            }
            pp.set("scale", v_scale);
            const std::string input_basename = boost::filesystem::path(input_file).filename().string();
            pp.set("input_filename", input_basename);
            const std::string input_basename_base = input_basename.substr(0, input_basename.find_last_of("."));
            pp.set("input_filename_base", input_basename_base);
        }
        return (boost::filesystem::path(input_file).parent_path() / this->output_filename()).string();
    }
    
    // if we were supplied a directory, use it and append our automatically generated filename
    boost::filesystem::path p(path);
    if (boost::filesystem::is_directory(p))
        return (p / this->output_filename()).string();
    
    // if we were supplied a file which is not a directory, use it
    return path;
}

}
