
#include "rest_conserve.hpp"

#include "types.hpp"

TaskStatus InitializeRest(MeshBlockData<Real> *rc, ParameterInput *pin)
{
    Flag("Initializing Rest Electron Heating problem");
    auto pmb = rc->GetBlockPointer();

    bool set_tlim = pin->GetOrAddBoolean("rest", "set_tlim", false);
    const Real u0 = pin->GetOrAddReal("rest", "u0", 1.);
    const Real rho0 = pin->GetOrAddReal("rest", "rho0", 1.);
    const Real v0 = pin->GetOrAddReal("rest", "v0", 1.);
    const Real q = (pin->DoesParameterExist("rest", "q")) ? pin->GetReal("rest", "q") : 0. ;
    bool context_boundaries = pin->GetOrAddBoolean("rest", "context_boundaries", false);
    // Time it would take for u to change by half its original value
    Real dyntimes = pin->GetOrAddReal("rest", "dyntimes", 0.5);

    Params& g_params = pmb->packages.Get("GRMHD")->AllParams();
    if(!g_params.hasKey("rho0")) g_params.Add("rho0", rho0);
    if(!g_params.hasKey("v0")) g_params.Add("v0", v0);
    if(!g_params.hasKey("u0")) g_params.Add("u0", u0);
    if(!g_params.hasKey("q")) g_params.Add("q", q);
    if(!g_params.hasKey("context_boundaries")) g_params.Add("context_boundaries", context_boundaries);

    // This is how we will initialize kel values later
    if (pmb->packages.AllPackages().count("Electrons")) {
        const Real fel0 = pmb->packages.Get("Electrons")->Param<Real>("fel_0");
        const Real game = pmb->packages.Get("Electrons")->Param<Real>("gamma_e");
        if(!g_params.hasKey("ke0")) g_params.Add("ke0", (game - 1.) * fel0 * u0 * pow(rho0, -game));
    }

    // Avoiding diving by zero and going into negative internal energy
    if (set_tlim && q != 0 && !(q < 0 && dyntimes > 1)) { 
        pin->SetReal("parthenon/time", "tlim", dyntimes*u0/abs(q));
    }

    SetRest(rc);

    Flag("Initialized");
    return TaskStatus::complete;
}

TaskStatus SetRest(MeshBlockData<Real> *rc, IndexDomain domain, bool coarse)
{
    Flag("Setting zones to Rest");
    auto pmb = rc->GetBlockPointer();
    GridScalar rho = rc->Get("prims.rho").data;
    GridScalar u = rc->Get("prims.u").data;
    GridVector uvec = rc->Get("prims.uvec").data;

    const Real u0 = pmb->packages.Get("GRMHD")->Param<Real>("u0");
    const Real v0 = pmb->packages.Get("GRMHD")->Param<Real>("v0");
    const Real rho0 = pmb->packages.Get("GRMHD")->Param<Real>("rho0");
    const Real q = pmb->packages.Get("GRMHD")->Param<Real>("q");
    const bool context_boundaries = pmb->packages.Get("GRMHD")->Param<bool>("context_boundaries");

    const Real tt = pmb->packages.Get("Globals")->Param<Real>("time");
    const Real dt = pmb->packages.Get("Globals")->Param<Real>("dt_last");
    const auto& G = pmb->coords;

    IndexRange ib = pmb->cellbounds.GetBoundsI(domain);
    IndexRange jb = pmb->cellbounds.GetBoundsJ(domain);
    IndexRange kb = pmb->cellbounds.GetBoundsK(domain);
    pmb->par_for("rest_init", kb.s, kb.e, jb.s, jb.e, ib.s, ib.e,
        KOKKOS_LAMBDA_3D {
            Real X[GR_DIM];
            G.coord_embed(k, j, i, Loci::center, X);
            rho(k, j, i) = rho0;
            u(k, j, i) = u0;
            uvec(0, k, j, i) = v0;
            uvec(1, k, j, i) = 0.0;
            uvec(2, k, j, i) = 0.0;
        }
    );

    if (pmb->packages.AllPackages().count("Electrons")) {
        GridScalar ktot = rc->Get("prims.Ktot").data;
        GridScalar kel_const = rc->Get("prims.Kel_Constant").data;
        const Real game = pmb->packages.Get("Electrons")->Param<Real>("gamma_e");
        const Real ke0 = pmb->packages.Get("GRMHD")->Param<Real>("ke0");
        pmb->par_for("rest_init", kb.s, kb.e, jb.s, jb.e, ib.s, ib.e,
            KOKKOS_LAMBDA_3D {
                ktot(k, j, i) = ke0;
                kel_const(k, j, i) = ke0; 
            }
        );
    }
    Flag("Set");
    return TaskStatus::complete;
}
