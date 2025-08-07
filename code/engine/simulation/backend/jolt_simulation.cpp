#undef global
#undef function
#undef Clamp
#undef Abs
#include <Jolt/Jolt.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/ScaledShape.h>
#define global static
#define function static
#define Clamp(min, v, max) Min(Max(min, v), max)
#define Abs(v) (((v) < 0) ? -(v) : (v))

using namespace JPH;

#define JOLT_BARRIER_CAPACITY 1024
class jolt_job_system : public JobSystem {
public:
	class jolt_job : public Job {
	public:
		u32 	   Index = 0;
		atomic_b32 IsFinished = {};
		jolt_job*  Next = NULL;

		jolt_job(const char* inJobName, ColorArg inColor, JobSystem* inJobSystem, const JobFunction& inJobFunction, uint32 inNumDependencies) : Job(inJobName, inColor, inJobSystem, inJobFunction, inNumDependencies) {}
	
		void* operator new(size_t Size, void* Ptr) {
			return Ptr;
		}
	};

	class jolt_barrier : public Barrier {
	public:
		u32 	   Index = 0;
		atomic_ptr JobList = {};

		void* operator new(size_t Size, void* Ptr) {
			return Ptr;
		}

		virtual void AddJob(const JobHandle &inJob) override {
			AddJobs(&inJob, 1);
		}

		virtual void AddJobs(const JobHandle *inHandles, uint inNumHandles) override {
			for (uint i = 0; i < inNumHandles; i++) {
				jolt_job* Job = (jolt_job*)inHandles[i].GetPtr();
				if (Job->SetBarrier(this)) {
					
					Job->AddRef();

					for(;;) {
						jolt_job* OldTop = (jolt_job*)Atomic_Load_Ptr(&JobList);
						Job->Next = OldTop;
						if(Atomic_Compare_Exchange_Ptr(&JobList, OldTop, Job) == OldTop) {
							break;
						}
					}
				}
			}
		}
	protected:
		virtual void OnJobFinished(Job *inJob) override {
			jolt_job* Job = (jolt_job*)inJob;
			Atomic_Store_B32(&Job->IsFinished, true);
		}
	};

	job_system* System;
	jolt_job* Jobs;
	async_stack_index32 FreeJobIndices;
	
	async_stack_index32 FreeBarrierIndices;
	jolt_barrier* Barriers;
	
	jolt_job_system() {
		System = Get_Engine()->JobSystem;
		arena* Arena = Simulation_Get()->Arena;
		u32 Capacity = Job_System_Capacity(System);
		
		Jobs = Arena_Push_Array(Arena, Capacity, jolt_job);
		u32* JobIndicesPtr = Arena_Push_Array(Arena, Capacity, u32);
		Async_Stack_Index32_Init_Raw(&FreeJobIndices, JobIndicesPtr, Capacity);
		for(u32 i = 0; i < Capacity; i++) {
			Async_Stack_Index32_Push_Sync(&FreeJobIndices, i);
		}

		Barriers = Arena_Push_Array(Arena, JOLT_BARRIER_CAPACITY, jolt_barrier);
		u32* BarrierIndicesPtr = Arena_Push_Array(Arena, JOLT_BARRIER_CAPACITY, u32);
		Async_Stack_Index32_Init_Raw(&FreeBarrierIndices, BarrierIndicesPtr, JOLT_BARRIER_CAPACITY);
		for(u32 i = 0; i < JOLT_BARRIER_CAPACITY; i++) {
			Async_Stack_Index32_Push_Sync(&FreeBarrierIndices, i);
		}
	}

	jolt_job* Allocate_Job(const char *inName, ColorArg inColor, const JobFunction &inJobFunction, uint32 inNumDependencies) {
		/*Need to find a free job index atomically*/
		u32 FreeIndex = Async_Stack_Index32_Pop(&FreeJobIndices);
		if(FreeIndex == ASYNC_STACK_INDEX32_INVALID) {
			/*No more jobs avaiable*/
			Assert(false);
			return NULL;
		}

		jolt_job* Job = Jobs+FreeIndex;


		new(Job) jolt_job(inName, inColor, this, inJobFunction, inNumDependencies);

		Job->Index = FreeIndex;

		return Job;  
	}

	function JOB_CALLBACK_DEFINE(Job_Callback) {
		jolt_job* Job = *(jolt_job**)UserData;
		Job->Execute();
		Job->Release();
	}

	virtual int GetMaxConcurrency() const override {
		return System->ThreadCount;
	}

	virtual JobHandle CreateJob(const char *inName, ColorArg inColor, const JobFunction &inJobFunction, uint32 inNumDependencies = 0) override {
		jolt_job* Job = Allocate_Job(inName, inColor, inJobFunction, inNumDependencies);
		Assert(Job->GetJobSystem() == this);

		JobHandle Result(Job);

		if (!inNumDependencies) {
			QueueJob(Job);
		}

		return Result;
	}

	/// Create a new barrier, used to wait on jobs
	virtual Barrier* CreateBarrier() override {
		u32 FreeIndex = Async_Stack_Index32_Pop(&FreeBarrierIndices);
		if (FreeIndex == ASYNC_STACK_INDEX32_INVALID) {
			/*No more barriers avaiable*/
			Assert(false);
			return NULL;
		}

		jolt_barrier* Barrier = Barriers + FreeIndex;

		new(Barrier) jolt_barrier;
		Barrier->Index = FreeIndex;

		return Barrier;
	}

	/// Destroy a barrier when it is no longer used. The barrier should be empty at this point.
	virtual void DestroyBarrier(Barrier* inBarrier) override {
		jolt_barrier* Barrier = (jolt_barrier*)inBarrier;
		Async_Stack_Index32_Push(&FreeBarrierIndices, Barrier->Index);
	}

	/// Wait for a set of jobs to be finished, note that only 1 thread can be waiting on a barrier at a time
	virtual void WaitForJobs(Barrier *inBarrier) override {
		jolt_barrier* Barrier = (jolt_barrier*)inBarrier;
		
		
		for (;;) {
			b32 IsWaiting = false;
			for (jolt_job* Job = (jolt_job*)Atomic_Load_Ptr(&Barrier->JobList); Job; Job = Job->Next) {
				if (!Atomic_Load_B32(&Job->IsFinished)) {
					IsWaiting = true;
				}
			}

			if (IsWaiting) {
				Job_System_Process_One_Job_And_Yield(System);
			} else {
				break;
			} 
		}

		for (jolt_job* Job = (jolt_job*)Atomic_Load_Ptr(&Barrier->JobList); Job; Job = Job->Next) {
			Job->Release();
		}

		Atomic_Store_Ptr(&Barrier->JobList, NULL);
	}

protected:
	/// Adds a job to the job queue
	virtual void QueueJob(Job* inJob) override {
		QueueJobs(&inJob, 1);
	}

	/// Adds a number of jobs at once to the job queue
	virtual void QueueJobs(Job** inJobs, uint inNumJobs) override {
		for (uint i = 0; i < inNumJobs; i++) {
			jolt_job* Job = (jolt_job*)inJobs[i];
			Job->AddRef();

			job_data JobData = {
				.JobCallback = Job_Callback,
				.Data = &Job, 
				.DataByteSize = sizeof(jolt_job*)
			};

			Job_System_Alloc_Job(System, JobData, Job_ID_Empty(), JOB_FLAG_FREE_WHEN_DONE_BIT | JOB_FLAG_QUEUE_IMMEDIATELY_BIT);
		}
	}

	/// Frees a job
	virtual void FreeJob(Job *inJob) override {
		jolt_job* Job = (jolt_job*)inJob;
		Async_Stack_Index32_Push(&FreeJobIndices, Job->Index);
	}
};

#include "jolt_simulation.h"

function inline Vec3 Jolt_V3(v3 V) {
	Vec3 Result(V.x, V.y, V.z);
	return Result;
}

function inline JPH::Quat Jolt_Quat(quat Q) {
	JPH::Quat Result(Q.x, Q.y, Q.z, Q.w);
	return Result;
}

function inline v3 V3_From_Jolt(const Vec3& v) {
	v3 Result = V3(v.GetX(), v.GetY(), v.GetZ());
	return Result;
}

function inline quat Quat_From_Jolt(const JPH::Quat& q) {
	quat Result = ::Quat(q.GetX(), q.GetY(), q.GetZ(), q.GetW());
	return Result;
}

function SIMULATION_BACKEND_CREATE_BODY(Jolt_Backend_Create_Body) {
	jolt_backend* Backend = (jolt_backend*)Simulation->Backend;
	BodyInterface* BodyInterface = &Backend->System.GetBodyInterfaceNoLock();

	pool_id ID = Pool_Allocate(&Backend->Bodies);
	sim_body* Result = (sim_body*)Pool_Get(&Backend->Bodies, ID);

	const sim_collider* Collider = &CreateInfo.Collider;

	Ref<Shape> ShapeRef;
	switch (Collider->Type) {
		case SIM_COLLIDER_TYPE_SPHERE: {
			SphereShapeSettings Settings(Collider->Sphere.Radius);
			Settings.SetDensity(Collider->Sphere.Density);
			ShapeRef = Settings.Create().Get();
		} break;

		case SIM_COLLIDER_TYPE_BOX: {
			BoxShapeSettings Settings(Jolt_V3(Collider->Box.HalfExtent));
			Settings.SetDensity(Collider->Box.Density);
			ShapeRef = Settings.Create().Get();
		} break;

		Invalid_Default_Case;
	}

	if (CreateInfo.Scale.x != 1 || CreateInfo.Scale.y != 1 || CreateInfo.Scale.z != 1) {
		ShapeRef = new ScaledShape(ShapeRef, Jolt_V3(CreateInfo.Scale));
	}

	EMotionType MotionType;
	switch (CreateInfo.BodyType) {
		case SIM_BODY_STATIC: {
			MotionType = EMotionType::Static;
		} break;

		case SIM_BODY_DYNAMIC: {
			MotionType = EMotionType::Dynamic;
		} break;
	}
	
	BodyCreationSettings BodyInfo(ShapeRef, Jolt_V3(CreateInfo.Position), 
								  Jolt_Quat(CreateInfo.Orientation), MotionType, 0);
	BodyInfo.mFriction = CreateInfo.Friction;
	BodyInfo.mUserData = (u64)(size_t)CreateInfo.UserData;

	Result->Body = BodyInterface->CreateBody(BodyInfo);
	BodyInterface->AddBody(Result->Body->GetID(), EActivation::Activate);

	return Result;
}

function SIMULATION_BACKEND_UPDATE(Jolt_Backend_Update) {
	jolt_backend* Backend = (jolt_backend*)Simulation->Backend;
	jolt_temp_allocator TempAllocator;

	BodyInterface* BodyInterface = &Backend->System.GetBodyInterface();
	for (size_t i = 0; i < Simulation->Entities.Capacity; i++) {
		sim_entity* SimEntity = Simulation->Entities.Entities + i;
		if (SimEntity->IsAllocated) {
			Body* Body = SimEntity->Body->Body;

			BodyInterface->SetPositionAndRotation(Body->GetID(), Jolt_V3(SimEntity->Position), Jolt_Quat(SimEntity->Orientation), EActivation::Activate);
			if (SimEntity->BodyType != SIM_BODY_STATIC) {
				Body->SetLinearVelocity(Jolt_V3(SimEntity->LinearVelocity));
				Body->SetAngularVelocity(Jolt_V3(SimEntity->AngularVelocity));
				Body->AddForce(Jolt_V3(SimEntity->Force));
				Body->AddTorque(Jolt_V3(SimEntity->Torque));
			}
		}
	}

	Backend->System.Update(dt, 1, &TempAllocator, &Backend->JobSystem);

	for (size_t i = 0; i < Simulation->Entities.Capacity; i++) {
		sim_entity* SimEntity = Simulation->Entities.Entities + i;
		if (SimEntity->IsAllocated) {
			Body* Body = SimEntity->Body->Body;

			SimEntity->Position = V3_From_Jolt(Body->GetPosition());
			SimEntity->Orientation = Quat_From_Jolt(Body->GetRotation());
			if (SimEntity->BodyType != SIM_BODY_STATIC) {
				SimEntity->LinearVelocity = V3_From_Jolt(Body->GetLinearVelocity());
				SimEntity->AngularVelocity = V3_From_Jolt(Body->GetAngularVelocity());
				SimEntity->Force = V3_Zero();
				SimEntity->Torque = V3_Zero();
			}
		}
	}
}

function SIMULATION_BACKEND_SET_GRAVITY(Jolt_Backend_Set_Gravity) {
	jolt_backend* Backend = (jolt_backend *)Simulation->Backend;
	Backend->System.SetGravity(Jolt_V3(Gravity));
}

global simulation_backend_vtable VTable = {
	.CreateBodyFunc = Jolt_Backend_Create_Body,
	.UpdateFunc = Jolt_Backend_Update,
	.SetGravityFunc = Jolt_Backend_Set_Gravity
};

function void* Jolt_Backend_Allocate_Memory(size_t Size) {
	void* Ptr = Allocator_Allocate_Memory(Default_Allocator_Get(), Size);
	return Ptr;
}

function void* Jolt_Backend_Reallocate_Memory(void* Block, size_t OldSize, size_t NewSize) {
	if (OldSize == NewSize) return Block;

	void* Ptr = Allocator_Allocate_Memory(Default_Allocator_Get(), NewSize);
	if (Ptr) {
		Memory_Copy(Ptr, Block, Min(OldSize, NewSize));
		Allocator_Free_Memory(Default_Allocator_Get(), Block);
	}

	return Ptr;
}

function void Jolt_Backend_Free_Memory(void* Block) {
	Allocator_Free_Memory(Default_Allocator_Get(), Block);
}

function void* Jolt_Backend_Allocate_Aligned_Memory(size_t Size, size_t Alignment) {
	Assert(Alignment > 0 && Is_Pow2(Alignment));

	size_t Offset = Alignment - 1 + sizeof(void*);
	void* P1 = Allocator_Allocate_Memory(Default_Allocator_Get(), Size + Offset);
	if (!P1) return NULL;

    void** P2 = (void**)(((size_t)(P1) + Offset) & ~(Alignment - 1));
    P2[-1] = P1;
        
    return P2;
}

function void Jolt_Backend_Free_Aligned_Memory(void* Memory) {
	if (Memory) {
		void* OriginalUnaligned = ((void* *)Memory)[-1];
		Allocator_Free_Memory(Default_Allocator_Get(), OriginalUnaligned);
	}
}

function void Simulation_Init_Backend(simulation* Simulation) {
	JPH::Allocate = Jolt_Backend_Allocate_Memory;
	JPH::Reallocate = Jolt_Backend_Reallocate_Memory;
	JPH::Free = Jolt_Backend_Free_Memory;
	JPH::AlignedAllocate = Jolt_Backend_Allocate_Aligned_Memory;
	JPH::AlignedFree = Jolt_Backend_Free_Aligned_Memory;


	JPH::Trace = [](const char* inFMT, ...) {
		arena* Scratch = Scratch_Get();
		
		va_list List;
		va_start(List, inFMT);
		string String = String_FormatV((allocator*)Scratch, inFMT, List);
		va_end(List);

		Debug_Log("%.*s", String.Size, String.Ptr);
		Scratch_Release();
    };

    JPH::Factory::sInstance = new JPH::Factory();
    JPH::RegisterTypes();
	
	jolt_backend* Backend = new jolt_backend;

  	Backend->VTable = &VTable;
	Simulation->Backend = Backend;
	Backend->Bodies = Pool_Init(sizeof(sim_body));

	Backend->System.Init(JOLT_MAX_NUM_BODIES, 0, JOLT_MAX_BODY_PAIRS,
						 JOLT_MAX_CONTACT_CONSTRAINTS, Backend->BroadPhaseLayers, 
						 Backend->BroadPhaseLayerFilters, Backend->LayerPairFilter);	
	
	PhysicsSettings Settings = Backend->System.GetPhysicsSettings();
	Settings.mNumVelocitySteps = SIMULATION_VELOCITY_ITERATIONS;
	Settings.mNumPositionSteps = SIMULATION_POSITION_ITERATIONS;
	Backend->System.SetPhysicsSettings(Settings);
}

function void Simulation_Reload(simulation* Simulation) {
	JPH::Allocate = Jolt_Backend_Allocate_Memory;
	JPH::Reallocate = Jolt_Backend_Reallocate_Memory;
	JPH::Free = Jolt_Backend_Free_Memory;
	JPH::AlignedAllocate = Jolt_Backend_Allocate_Aligned_Memory;
	JPH::AlignedFree = Jolt_Backend_Free_Aligned_Memory;

	JPH::Factory::sInstance = new JPH::Factory();
    JPH::RegisterTypes();
}

#pragma comment(lib, "Jolt.lib")