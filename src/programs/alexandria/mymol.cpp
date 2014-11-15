/*
 * This file is part of the GROMACS molecular simulation package.
 *
 * Copyright (c) 2014, by the GROMACS development team, led by
 * Mark Abraham, David van der Spoel, Berk Hess, and Erik Lindahl,
 * and including many others, as listed in the AUTHORS file in the
 * top-level source directory and at http://www.gromacs.org.
 *
 * GROMACS is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1
 * of the License, or (at your option) any later version.
 *
 * GROMACS is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with GROMACS; if not, see
 * http://www.gnu.org/licenses, or write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA.
 *
 * If you want to redistribute modifications to GROMACS, please
 * consider that scientific software is very special. Version
 * control is crucial - bugs must be traceable. We will be happy to
 * consider code for inclusion in the official distribution, but
 * derived work must not be called official GROMACS. Details are found
 * in the README & COPYING files - if they are missing, get the
 * official version at http://www.gromacs.org.
 *
 * To help us fund GROMACS development, we humbly ask that you cite
 * the research papers on the package. Check out http://www.gromacs.org.
 */
/*! \internal \brief
 * Implements part of the alexandria program.
 * \author David van der Spoel <david.vanderspoel@icm.uu.se>
 */
#include "gmxpre.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include "gromacs/listed-forces/bonded.h"
#include "gromacs/fileio/confio.h"
#include "gromacs/fileio/pdbio.h"
#include "gromacs/gmxpreprocess/pdb2top.h"
#include "gromacs/gmxpreprocess/toputil.h"
#include "gromacs/gmxpreprocess/gen_ad.h"
#include "gromacs/gmxpreprocess/gpp_atomtype.h"
#include "gromacs/gmxpreprocess/topdirs.h"
#include "gromacs/gmxpreprocess/convparm.h"
#include "gromacs/gmxpreprocess/gpp_nextnb.h"
#include "gromacs/legacyheaders/copyrite.h"
#include "gromacs/legacyheaders/force.h"
#include "gromacs/math/vec.h"
#include "gromacs/pbcutil/pbc.h"
#include "gromacs/utility/fatalerror.h"
#include "gromacs/utility/futil.h"
#include "gromacs/utility/smalloc.h"
#include "gromacs/legacyheaders/macros.h"
#include "gromacs/topology/mtop_util.h"
#include "gromacs/legacyheaders/shellfc.h"
#include "gromacs/legacyheaders/mdatoms.h"
#include "gromacs/topology/symtab.h"
#include "gromacs/fileio/filenm.h"
#include "gromacs/math/vec.h"
#include "poldata.h"
#include "poldata_xml.h"
#include "gmx_simple_comm.h"
#include "gentop_vsite.h"
#include "gentop_core.h"
#include "molprop.h"
#include "molprop_util.h"
#include "molprop_xml.h"
#include "gauss_io.h"
#include "mymol.h"
#include "stringutil.h"

static const char *gentop_version = "gentop 0.98";

#define STRLEN 256

static void get_force_constants(gmx_poldata_t pd, t_params plist[], t_atoms *atoms)
{
    int    j, n, ft, k;
    double xx, sx, bo;
    char  *params;

#define ATP(ii) ((char *)gmx_poldata_atype_to_btype(pd, *atoms->atomtype[ii]))
    ft = gmx_poldata_get_bond_ftype(pd);
    for (j = 0; (j < plist[ft].nr); j++)
    {
        if (0 < gmx_poldata_search_bond(pd,
                                        ATP(plist[ft].param[j].a[0]),
                                        ATP(plist[ft].param[j].a[1]),
                                        &xx, &sx, NULL, &bo, &params))
        {
            plist[ft].param[j].c[0] = convert2gmx(xx, eg2cPm);
            std::vector<std::string> ptr = split(params, ' ');
            n = 0;
            for (std::vector<std::string>::iterator pi = ptr.begin(); (pi < ptr.end()); ++pi)
            {
                if ((pi->length() > 0) && (n < MAXFORCEPARAM-1))
                {
                    plist[ft].param[j].c[1+n] = atof(pi->c_str());
                    n++;
                }
            }
        }
    }
    ft = gmx_poldata_get_angle_ftype(pd);
    for (j = 0; (j < plist[ft].nr); j++)
    {
        if (0 < gmx_poldata_search_angle(pd,
                                         ATP(plist[ft].param[j].a[0]),
                                         ATP(plist[ft].param[j].a[1]),
                                         ATP(plist[ft].param[j].a[2]),
                                         &xx, &sx, NULL, &params))
        {
            plist[ft].param[j].c[0] = xx;
            std::vector<std::string> ptr = split(params, ' ');
            n = 0;
            for (std::vector<std::string>::iterator pi = ptr.begin(); (pi < ptr.end()); ++pi)
            {
                if ((pi->length() > 0) && (n < MAXFORCEPARAM-1))
                {
                    plist[ft].param[j].c[1+n] = atof(pi->c_str());
                    n++;
                }
            }
        }
    }
    for (k = 0; (k < egdNR); k++)
    {
        ft = gmx_poldata_get_dihedral_ftype(pd, k);
        for (j = 0; (j < plist[ft].nr); j++)
        {
            if (0 < gmx_poldata_search_dihedral(pd, k,
                                                ATP(plist[ft].param[j].a[0]),
                                                ATP(plist[ft].param[j].a[1]),
                                                ATP(plist[ft].param[j].a[2]),
                                                ATP(plist[ft].param[j].a[3]),
                                                &xx, &sx, NULL, &params))
            {
                plist[ft].param[j].c[0] = xx;
                std::vector<std::string> ptr = split(params, ' ');
                n = 0;
                for (std::vector<std::string>::iterator pi = ptr.begin(); (pi < ptr.end()); ++pi)
                {
                    if ((pi->length() > 0) && (n < MAXFORCEPARAM-1))
                    {
                        plist[ft].param[j].c[1+n] = atof(pi->c_str());
                        n++;
                    }
                }
            }
        }
    }
}

namespace alexandria
{

const char *immsg(immStatus imm)
{
    static const char *msg[immNR] = {
        "Unknown status",
        "OK", "Zero Dipole", "No Quadrupole", "Charged",
        "Atom type problem", "Atom number problem", "Converting from molprop",
        "Determining bond order", "RESP Initialization",
        "Charge generation", "Requested level of theory missing",
        "QM Inconsistency (ESP dipole does not match Elec)",
        "Not in training set", "No experimental data",
        "Generating shells", "Generating bonds", "Communicating MolProp"
    };

    return msg[imm];
}

static void mv_plist(t_params *dst, t_params *src)
{
    int i;

    if (dst->maxnr < src->nr)
    {
        srenew(dst->param, src->nr);
        dst->maxnr = src->nr;
    }
    for (i = 0; (i < src->nr); i++)
    {
        cp_param(&dst->param[i], &src->param[i]);
    }
    dst->nr = src->nr;

    src->nr = 0;
}

static void mv_plists(gmx_poldata_t pd, t_params plist[], bool bForward)
{
    int ft;

    /* Now move over to the appropriate function types */
    if (NOTSET == (ft = gmx_poldata_get_bond_ftype(pd)))
    {
        gmx_fatal(FARGS, "Bond function type not set in force field file");
    }
    if (F_BONDS != ft)
    {
        if (bForward)
        {
            mv_plist(&plist[ft], &plist[F_BONDS]);
        }
        else
        {
            mv_plist(&plist[F_BONDS], &plist[ft]);
        }
    }
    if (NOTSET == (ft = gmx_poldata_get_angle_ftype(pd)))
    {
        gmx_fatal(FARGS, "Angle function type not set in force field file");
    }
    if (F_ANGLES != ft)
    {
        if (bForward)
        {
            mv_plist(&plist[ft], &plist[F_ANGLES]);
        }
        else
        {
            mv_plist(&plist[F_ANGLES], &plist[ft]);
        }
    }
    if (NOTSET == (ft = gmx_poldata_get_dihedral_ftype(pd, egdPDIHS)))
    {
        gmx_fatal(FARGS, "Dihedral function type not set in force field file");
    }
    if (F_PDIHS != ft)
    {
        if (bForward)
        {
            mv_plist(&plist[ft], &plist[F_PDIHS]);
        }
        else
        {
            mv_plist(&plist[F_PDIHS], &plist[ft]);
        }
    }
    if (NOTSET == (ft = gmx_poldata_get_dihedral_ftype(pd, egdIDIHS)))
    {
        gmx_fatal(FARGS, "Improper function type not set in force field file");
    }
    if (F_IDIHS != ft)
    {
        if (bForward)
        {
            mv_plist(&plist[ft], &plist[F_IDIHS]);
        }
        else
        {
            mv_plist(&plist[F_IDIHS], &plist[ft]);
        }
    }
}

static void detect_rings(t_params *bonds, int natom, gmx_bool bRing[])
{
    /* Check for 4,5,6,7,8 rings.
     */
    int j, k, l, m, n, o, p, q, a1, a2, a3, a4, a5, a6, a7, a8, a9;

    for (j = 0; (j < natom); j++)
    {
        bRing[j] = FALSE;
    }

    for (a1 = 0; (a1 < natom); a1++)
    {
        for (j = 0; (j < bonds->nr); j++)
        {
            a2 = NOTSET;
            if (bonds->param[j].a[0] == a1)
            {
                a2 = bonds->param[j].a[1];
            }
            else if (bonds->param[j].a[1] == a1)
            {
                a2 = bonds->param[j].a[0];
            }
            if (a2 != NOTSET)
            {
                for (k = 0; (k < bonds->nr); k++)
                {
                    a3 = NOTSET;
                    if (bonds->param[k].a[0] == a2)
                    {
                        a3 = bonds->param[k].a[1];
                    }
                    else if (bonds->param[k].a[1] == a2)
                    {
                        a3 = bonds->param[k].a[0];
                    }
                    if ((a3 != NOTSET) && (a3 != a1))
                    {
                        for (l = 0; (l < bonds->nr); l++)
                        {
                            a4 = NOTSET;
                            if (bonds->param[l].a[0] == a3)
                            {
                                a4 = bonds->param[l].a[1];
                            }
                            else if (bonds->param[l].a[1] == a3)
                            {
                                a4 = bonds->param[l].a[0];
                            }
                            if ((a4 != NOTSET) && (a4 != a2))
                            {
                                for (m = 0; (m < bonds->nr); m++)
                                {
                                    a5 = NOTSET;
                                    if (bonds->param[m].a[0] == a4)
                                    {
                                        a5 = bonds->param[m].a[1];
                                    }
                                    else if (bonds->param[m].a[1] == a4)
                                    {
                                        a5 = bonds->param[m].a[0];
                                    }
                                    if ((a5 != NOTSET) && (a5 != a3))
                                    {
                                        if (a5 == a1)
                                        {
                                            /* 4-ring */
                                            bRing[a1] = bRing[a2] = bRing[a3] = bRing[a4] = TRUE;
                                        }
                                        else if (a3 != a1)
                                        {
                                            for (n = 0; (n < bonds->nr); n++)
                                            {
                                                a6 = NOTSET;
                                                if (bonds->param[n].a[0] == a5)
                                                {
                                                    a6 = bonds->param[n].a[1];
                                                }
                                                else if (bonds->param[n].a[1] == a5)
                                                {
                                                    a6 = bonds->param[n].a[0];
                                                }
                                                if ((a6 != NOTSET) && (a6 != a4))
                                                {
                                                    if (a6 == a1)
                                                    {
                                                        /* 5-ring */
                                                        bRing[a1] = bRing[a2] = bRing[a3] = bRing[a4] = bRing[a5] = TRUE;
                                                    }
                                                    else
                                                    {
                                                        for (o = 0; (o < bonds->nr); o++)
                                                        {
                                                            a7 = NOTSET;
                                                            if (bonds->param[o].a[0] == a6)
                                                            {
                                                                a7 = bonds->param[o].a[1];
                                                            }
                                                            else if (bonds->param[o].a[1] == a6)
                                                            {
                                                                a7 = bonds->param[o].a[0];
                                                            }
                                                            if ((a7 != NOTSET) && (a7 != a5))
                                                            {
                                                                if (a7 == a1)
                                                                {
                                                                    /* 6-ring */
                                                                    bRing[a1]             = bRing[a2] = bRing[a3] =
                                                                                bRing[a4] = bRing[a5] = bRing[a6] = TRUE;
                                                                }
                                                                else
                                                                {
                                                                    for (p = 0; (p < bonds->nr); p++)
                                                                    {
                                                                        a8 = NOTSET;
                                                                        if (bonds->param[p].a[0] == a7)
                                                                        {
                                                                            a8 = bonds->param[p].a[1];
                                                                        }
                                                                        else if (bonds->param[p].a[1] == a7)
                                                                        {
                                                                            a8 = bonds->param[p].a[0];
                                                                        }
                                                                        if ((a8 != NOTSET) && (a8 != a6))
                                                                        {
                                                                            if (a8 == a1)
                                                                            {
                                                                                /* 7-ring */
                                                                                bRing[a1]             = bRing[a2] = bRing[a3] =
                                                                                            bRing[a4] = bRing[a5] = bRing[a6] = bRing[a7] = TRUE;
                                                                            }
                                                                            else
                                                                            {
                                                                                for (q = 0; (q < bonds->nr); q++)
                                                                                {
                                                                                    a9 = NOTSET;
                                                                                    if (bonds->param[q].a[0] == a8)
                                                                                    {
                                                                                        a9 = bonds->param[q].a[1];
                                                                                    }
                                                                                    else if (bonds->param[q].a[1] == a8)
                                                                                    {
                                                                                        a9 = bonds->param[q].a[0];
                                                                                    }
                                                                                    if (a9 == a1)
                                                                                    {
                                                                                        /* 8-ring */
                                                                                        bRing[a1]             = bRing[a2] = bRing[a3] =
                                                                                                    bRing[a4] = bRing[a5] = bRing[a6] = bRing[a7] = bRing[a8] = TRUE;
                                                                                    }
                                                                                }
                                                                            }
                                                                        }
                                                                    }
                                                                }
                                                            }
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

static bool is_planar(rvec xi, rvec xj, rvec xk, rvec xl, t_pbc *pbc,
                      real phi_toler)
{
    int  t1, t2, t3;
    rvec r_ij, r_kj, r_kl, m, n;
    real sign, phi;

    phi = RAD2DEG*dih_angle(xi, xj, xk, xl, pbc, r_ij, r_kj, r_kl, m, n, &sign, &t1, &t2, &t3);

    return (fabs(phi) < phi_toler);
}

static bool is_linear(rvec xi, rvec xj, rvec xk, t_pbc *pbc,
                      real th_toler)
{
    int  t1, t2;
    rvec r_ij, r_kj;
    real costh, th;

    th = fabs(RAD2DEG*bond_angle(xi, xj, xk, pbc, r_ij, r_kj, &costh, &t1, &t2));

    return (th > th_toler) || (th < 180-th_toler);
}

void MyMol::MakeSpecialInteractions(bool bUseVsites, gmx_poldata_t pd)
{
    std::vector < std::vector < unsigned int> > bonds;
    std::vector<int> nbonds;
    t_pbc            pbc;
    matrix           box;
    real             th_toler = 5;
    real             ph_toler = 5;

    clear_mat(box);
    set_pbc(&pbc, epbcNONE, box);

    bonds.resize(topology_->atoms.nr);
    for (alexandria::BondIterator bi = BeginBond(); (bi < EndBond()); bi++)
    {
        bonds[bi->GetAi() - 1].push_back(bi->GetAj() - 1);
        bonds[bi->GetAj() - 1].push_back(bi->GetAi() - 1);
    }
    nbonds.resize(topology_->atoms.nr);
    for (int i = 0; (i < topology_->atoms.nr); i++)
    {
        nbonds[i] = bonds[i].size();
    }
    for (int i = 0; (i < topology_->atoms.nr); i++)
    {
        /* Now test initial geometry */
        if ((bonds[i].size() == 2) &&
            is_linear(x_[i], x_[bonds[i][0]], x_[bonds[i][1]],
                      &pbc, th_toler))
        {
            gvt.addLinear(bonds[i][0], i, bonds[i][1]);
        }
        else if ((bonds[i].size() == 3) &&
                 is_planar(x_[i], x_[bonds[i][0]],
                           x_[bonds[i][1]], x_[bonds[i][2]],
                           &pbc, ph_toler))
        {
            gvt.addPlanar(i, bonds[i][0], bonds[i][1], bonds[i][2],
                          &nbonds[0]);
        }
    }
    int anr = topology_->atoms.nr;

    gvt.generateSpecial(bUseVsites, &topology_->atoms, &x_, plist_,
                        symtab_, atype_, &excls_, pd);
    bHaveVSites_ = (topology_->atoms.nr > anr);
}

void MyMol::MakeAngles()
{
    t_nextnb nnb;
    t_restp  rtp;

    /* Make Angles and Dihedrals */
    snew(excls_, topology_->atoms.nr);
    init_nnb(&nnb, topology_->atoms.nr, nexcl_+2);
    gen_nnb(&nnb, plist_);
    //detect_rings(&plist_[F_BONDS],topology_->atoms.nr,bRing);

    print_nnb(&nnb, "NNB");
    rtp.bKeepAllGeneratedDihedrals    = TRUE;
    rtp.bRemoveDihedralIfWithImproper = TRUE;
    rtp.bGenerateHH14Interactions     = TRUE;
    rtp.nrexcl = nexcl_;
    gen_pad(&nnb, &(topology_->atoms), &rtp, plist_, excls_, NULL, FALSE);
    generate_excls(&nnb, nexcl_, excls_);
    done_nnb(&nnb);
}

static void generate_nbparam(int ftype, int comb, double ci[], double cj[],
                             t_iparams *ip)
{
    double sig, eps;

    switch (ftype)
    {
        case F_LJ:
            switch (comb)
            {
                case eCOMB_GEOMETRIC:
                    /* Gromos rules */
                    ip->lj.c6  = sqrt(ci[0] * cj[0]);
                    ip->lj.c12 = sqrt(ci[1] * cj[1]);
                    break;

                case eCOMB_ARITHMETIC:
                    /* c0 and c1 are epsilon and sigma */
                    sig        = (ci[0]+cj[0])*0.5;
                    eps        = sqrt(ci[1]*cj[1]);
                    ip->lj.c6  = 4*eps*pow(sig, 6);
                    ip->lj.c12 = 4*eps*pow(sig, 12);

                    break;
                case eCOMB_GEOM_SIG_EPS:
                    /* c0 and c1 are epsilon and sigma */
                    sig        = sqrt(ci[0]*cj[0]);
                    eps        = sqrt(ci[1]*cj[1]);
                    ip->lj.c6  = 4*eps*pow(sig, 6);
                    ip->lj.c12 = 4*eps*pow(sig, 12);

                    break;
                default:
                    gmx_fatal(FARGS, "No such combination rule %d", comb);
            }
            break;
        default:
            gmx_fatal(FARGS, "No such function type supported %s",
                      interaction_function[ftype].name);
    }
}

static void do_init_mtop(gmx_poldata_t pd,
                         gmx_mtop_t   *mtop_,
                         char        **molname,
                         t_atoms      *atoms)
{
    init_mtop(mtop_);
    mtop_->name     = molname;
    mtop_->nmoltype = 1;
    snew(mtop_->moltype, mtop_->nmoltype);
    mtop_->moltype[0].name = molname;
    mtop_->nmolblock       = 1;
    snew(mtop_->molblock, mtop_->nmolblock);
    mtop_->molblock[0].nmol        = 1;
    mtop_->molblock[0].type        = 0;
    mtop_->molblock[0].natoms_mol  = atoms->nr;
    mtop_->groups.grps[egcENER].nr = 1;

    //! Count the number of types in this molecule, at least 1 assuming there is one atom
    int ntype = 1;
    for (int i = 1; (i < atoms->nr); i++)
    {
        int  itp   = atoms->atom[i].type;
        bool found = false;
        for (int j = 0; !found && (j < i); j++)
        {
            found = (itp == atoms->atom[j].type);
        }
        if (!found)
        {
            ntype++;
        }
    }

    mtop_->ffparams.atnr   = ntype;
    mtop_->ffparams.ntypes = ntype*ntype;
    mtop_->ffparams.reppow = 12;

    int vdw_type = gmx_poldata_get_vdw_ftype(pd);

    snew(mtop_->ffparams.functype, mtop_->ffparams.ntypes);
    snew(mtop_->ffparams.iparams, mtop_->ffparams.ntypes);
    for (int i = 0; (i < ntype); i++)
    {
        for (int j = 0; (j < ntype); j++)
        {
            int idx = ntype*i+j;
            mtop_->ffparams.functype[idx] = vdw_type;
            switch (vdw_type)
            {
                case F_LJ:
                    //! NOTE  get the real parameters from the pd here
                    //! May need to set the atomtypes properly too.
                    mtop_->ffparams.iparams[idx].lj.c6  = 0;
                    mtop_->ffparams.iparams[idx].lj.c12 = 0;
                    break;
                case F_BHAM:
                    mtop_->ffparams.iparams[idx].bham.a = 0;
                    mtop_->ffparams.iparams[idx].bham.b = 0;
                    mtop_->ffparams.iparams[idx].bham.c = 0;
                    break;
                default:
                    fprintf(stderr, "Invalid van der waals type %s\n",
                            gmx_poldata_get_vdw_function(pd));
            }
        }
    }

    /* Create a charge group block */
    stupid_fill_block(&(mtop_->moltype[0].cgs), atoms->nr, FALSE);

    mtop_->natoms = atoms->nr;
    init_t_atoms(&(mtop_->moltype[0].atoms), atoms->nr, FALSE);
}

static void excls__to_blocka(int natom, t_excls excls_[], t_blocka *blocka)
{
    int i, j, k, nra;

    if (blocka->nr < natom)
    {
        srenew(blocka->index, natom+1);
    }
    nra = 0;
    for (i = 0; (i < natom); i++)
    {
        nra += excls_[i].nr;
    }
    snew(blocka->a, nra+1);
    nra = 0;
    for (i = j = 0; (i < natom); i++)
    {
        blocka->index[i] = nra;
        for (k = 0; (k < excls_[i].nr); k++)
        {
            blocka->a[j++] = excls_[i].e[k];
        }
        nra += excls_[i].nr;
    }
    blocka->index[natom] = nra;
    blocka->nr           = natom;
    blocka->nra          = nra;
}

static void plist_to_mtop(gmx_poldata_t pd, t_params plist_[], gmx_mtop_t *mtop_)
{
    double fudgeLJ;
    double reppow = 12.0;
    int    n      = 0;

    /* Generate pairs */
    fudgeLJ = gmx_poldata_get_fudgeLJ(pd);

    int nfptot = mtop_->ffparams.ntypes;
    for (int i = 0; (i < F_NRE); i++)
    {
        nfptot += plist_[i].nr*NRFPA(i);
    }
    srenew(mtop_->ffparams.functype, nfptot);
    srenew(mtop_->ffparams.iparams, nfptot);

    for (int i = 0; (i < F_NRE); i++)
    {
        int nra    = NRAL(i);
        int nrfp   = NRFPA(i);
        int nratot = plist_[i].nr*(1+nra);
        snew(mtop_->moltype[0].ilist[i].iatoms, nratot);
        int k = 0;
        for (int j = 0; (j < plist_[i].nr); j++)
        {
            real c[MAXFORCEPARAM];
            int  l = 0;
            if (i == F_LJ14)
            {
                int ati = mtop_->moltype[0].atoms.atom[plist_[i].param[j].a[0]].type;
                int atj = mtop_->moltype[0].atoms.atom[plist_[i].param[j].a[1]].type;
                int tp  = ati*mtop_->ffparams.atnr+atj;
                c[l++] = mtop_->ffparams.iparams[tp].lj.c6*fudgeLJ;
                c[l++] = mtop_->ffparams.iparams[tp].lj.c12*fudgeLJ;
            }
            else
            {
                for (; (l < nrfp); l++)
                {
                    c[l] = plist_[i].param[j].c[l];
                    if (NOTSET == c[l])
                    {
                        c[l] = 0;
                    }
                }
            }
            for (; (l < MAXFORCEPARAM); l++)
            {
                c[l] = 0;
            }
            n = enter_params(&mtop_->ffparams, i, c, 0, reppow, n, TRUE);
            mtop_->moltype[0].ilist[i].iatoms[k++] = n;
            for (l = 0; (l < nra); l++)
            {
                mtop_->moltype[0].ilist[i].iatoms[k++] = plist_[i].param[j].a[l];
            }
        }
        mtop_->moltype[0].ilist[i].nr = k;
    }
}

void mtop_update_cgs(gmx_mtop_t *mtop)
{
    int i, j;

    for (i = 0; (i < mtop->nmoltype); i++)
    {
        if (mtop->moltype[i].atoms.nr > mtop->moltype[i].cgs.nr)
        {
            mtop->moltype[i].cgs.nr           = mtop->moltype[i].atoms.nr;
            mtop->moltype[i].cgs.nalloc_index = mtop->moltype[i].atoms.nr+1;
            srenew(mtop->moltype[i].cgs.index, mtop->moltype[i].cgs.nr+1);
            for (j = 0; (j <= mtop->moltype[i].cgs.nr); j++)
            {
                mtop->moltype[i].cgs.index[j] = j;
            }
        }
    }
}

bool MyMol::IsSymmetric(real toler)
{
    int       i, j, m;
    real      mm, tm;
    rvec      com, test;
    gmx_bool *bSymm, bSymmAll;

    clear_rvec(com);
    tm = 0;
    for (i = 0; (i < topology_->atoms.nr); i++)
    {
        mm  = topology_->atoms.atom[i].m;
        tm += mm;
        for (m = 0; (m < DIM); m++)
        {
            com[m] += mm*x_[i][m];
        }
    }
    if (tm > 0)
    {
        for (m = 0; (m < DIM); m++)
        {
            com[m] /= tm;
        }
    }
    for (i = 0; (i < topology_->atoms.nr); i++)
    {
        rvec_dec(x_[i], com);
    }

    snew(bSymm, topology_->atoms.nr);
    for (i = 0; (i < topology_->atoms.nr); i++)
    {
        bSymm[i] = (norm(x_[i]) < toler);
        for (j = i+1; (j < topology_->atoms.nr) && !bSymm[i]; j++)
        {
            rvec_add(x_[i], x_[j], test);
            if (norm(test) < toler)
            {
                bSymm[i] = TRUE;
                bSymm[j] = TRUE;
            }
        }
    }
    bSymmAll = TRUE;
    for (i = 0; (i < topology_->atoms.nr); i++)
    {
        bSymmAll = bSymmAll && bSymm[i];
    }
    sfree(bSymm);
    for (i = 0; (i < topology_->atoms.nr); i++)
    {
        rvec_inc(x_[i], com);
    }

    return bSymmAll;
}

static void fill_inputrec(t_inputrec *ir)
{
    ir->cutoff_scheme = ecutsGROUP;
    ir->tabext        = 2; /* nm */
    ir->ePBC          = epbcNONE;
    ir->epsilon_r     = 1;
    ir->vdwtype       = evdwCUT;
    ir->coulombtype   = eelCUT;
    ir->eDispCorr     = edispcNO;
    snew(ir->opts.egp_flags, 1);
    snew(ir->fepvals, 1);
}

MyMol::MyMol() //: MolProp()
{
    bHaveShells_       = false;
    bHaveVSites_       = false;
    cgnr_              = NULL;
    gr_                = NULL;
    immAtoms_          = immOK;
    immTopology_       = immOK;
    immCharges_        = immOK;
    snew(symtab_, 1);
    open_symtab(symtab_);
    atype_ = init_atomtype();
    for (int i = 0; (i < ebtsNR); i++)
    {
        bts[i] = NOTSET;
    }
    clear_mat(box);
    mtop_  = NULL;
    ltop_  = NULL;
    md_    = NULL;
    shell_ = NULL;

    init_enerdata(1, 0, &enerd_);

    /* Inputrec parameters */
    snew(inputrec_, 1);
    fill_inputrec(inputrec_);

    /* Topology_ stuff */
    snew(plist_, F_NRE);
    init_plist(plist_);
}

MyMol::~MyMol()
{
    return;
    if (NULL != cgnr_)
    {
        sfree(cgnr_);
        cgnr_ = NULL;
    }
    if (NULL != atype_)
    {
        done_atomtype(atype_);
        atype_ = NULL;
    }
    if (NULL != inputrec_)
    {
        sfree(inputrec_);
        inputrec_ = NULL;
    }
    if (NULL != plist_)
    {
        for (int i = 0; (i < F_NRE); i++)
        {
            sfree(plist_[i].param);
            sfree(plist_[i].cmap);
            sfree(plist_[i].cmap_types);
        }
        sfree(plist_);
        plist_ = NULL;
    }
    if (NULL != symtab_)
    {
        done_symtab(symtab_);
        sfree(symtab_);
        symtab_ = NULL;
    }
}

immStatus MyMol::GenerateAtoms(gmx_atomprop_t        ap,
                               const char           *lot,
                               ChargeGenerationModel iModel)
{
    int                 myunit;
    double              xx, yy, zz;
    int                 natom;
    immStatus           imm   = immOK;

    CalculationIterator ci = GetLot(lot);
    if (ci < EndCalculation())
    {
        t_param nb;

        memset(&nb, 0, sizeof(nb));
        natom = 0;
        init_t_atoms(&(topology_->atoms), ci->NAtom(), FALSE);
        snew(x_, ci->NAtom());
        snew(topology_->atoms.atomtype, ci->NAtom());
        snew(topology_->atoms.atomtypeB, ci->NAtom());

        for (CalcAtomIterator cai = ci->BeginAtom(); (cai < ci->EndAtom()); cai++)
        {
            myunit = string2unit((char *)cai->GetUnit().c_str());
            if (myunit == -1)
            {
                gmx_fatal(FARGS, "Unknown unit '%s' for atom coords",
                          cai->GetUnit().c_str());
            }
            cai->GetCoords(&xx, &yy, &zz);
            x_[natom][XX] = convert2gmx(xx, myunit);
            x_[natom][YY] = convert2gmx(yy, myunit);
            x_[natom][ZZ] = convert2gmx(zz, myunit);

            double q = 0;
            for (AtomicChargeIterator qi = cai->BeginQ(); (qi < cai->EndQ()); qi++)
            {
                ChargeGenerationModel qtp = name2eemtype(qi->GetType().c_str());
                if (qtp == iModel)
                {
                    myunit = string2unit((char *)qi->GetUnit().c_str());
                    q      = convert2gmx(qi->GetQ(), myunit);
                    break;
                }
            }
            topology_->atoms.atom[natom].q      =
                topology_->atoms.atom[natom].qB = q;

            t_atoms_set_resinfo(&(topology_->atoms), natom, symtab_, GetMolname().c_str(), 1, ' ', 1, ' ');
            topology_->atoms.atomname[natom]        = put_symtab(symtab_, cai->GetName().c_str());
            topology_->atoms.atom[natom].atomnumber = gmx_atomprop_atomnumber(ap, cai->GetName().c_str());

            real mass = 0;
            if (!gmx_atomprop_query(ap, epropMass, "???", cai->GetName().c_str(), &mass))
            {
                fprintf(stderr, "Could not find mass for %s\n", cai->GetName().c_str());
            }
            topology_->atoms.atom[natom].m      =
                topology_->atoms.atom[natom].mB = mass;

            strcpy(topology_->atoms.atom[natom].elem, gmx_atomprop_element(ap, topology_->atoms.atom[natom].atomnumber));

            topology_->atoms.atom[natom].resind = 0;
            // First set the atomtype
            topology_->atoms.atomtype[natom]      =
                topology_->atoms.atomtypeB[natom] = put_symtab(symtab_, cai->GetObtype().c_str());

            natom++;
        }
        /* Change their atomtype from the OpenBabel internal type to the
         * one specified in our force field file (gentop.dat).
         */
        //translate_atomtypes(&topology_->atoms,&(symtab_),
        //                  gmx_poldata_get_force_field(pd));
        for (int i = 0; (i < natom); i++)
        {
            topology_->atoms.atom[i].type      =
                topology_->atoms.atom[i].typeB = add_atomtype(atype_, symtab_,
                                                              &(topology_->atoms.atom[i]),
                                                              *topology_->atoms.atomtype[i],
                                                              &nb,
                                                              0, 0.0, 0.0, 0.0,
                                                              topology_->atoms.atom[i].atomnumber,
                                                              0.0, 0.0);
        }
        topology_->atoms.nr   = natom;
        topology_->atoms.nres = 1;
    }
    else
    {
        imm = immLOT;
    }
    if (NULL != debug)
    {
        fprintf(debug, "Tried to convert %s to gromacs. LOT is %s. Natoms is %d\n",
                GetMolname().c_str(), lot, natom);
    }

    return imm;
}

immStatus MyMol::GenerateTopology(gmx_atomprop_t        ap,
                                  gmx_poldata_t         pd,
                                  const char           *lot,
                                  ChargeGenerationModel iModel,
                                  int                   nexcl,
                                  bool                  bUseVsites,
                                  bool                  bPairs,
                                  eDih                  edih)
{
    immStatus                imm = immOK;
    int                      ftb;
    t_param                  b;

    if (NULL != debug)
    {
        fprintf(debug, "Generating topology_ for %s\n", GetMolname().c_str());
    }

    /* Set bts for topology_ output */
    if (NOTSET == (bts[ebtsBONDS]  = gmx_poldata_get_bond_ftype(pd)))
    {
        gmx_fatal(FARGS, "No bonded type defined in force field file");
    }
    if (NOTSET == (bts[ebtsANGLES] = gmx_poldata_get_angle_ftype(pd)))
    {
        gmx_fatal(FARGS, "No angle type defined in force field file");
    }
    if (NOTSET == (bts[ebtsIDIHS]  = gmx_poldata_get_dihedral_ftype(pd, egdIDIHS)))
    {
        gmx_fatal(FARGS, "No improper dihedral type defined in force field file");
    }
    if (NOTSET == (bts[ebtsPDIHS]  = gmx_poldata_get_dihedral_ftype(pd, egdPDIHS)))
    {
        gmx_fatal(FARGS, "No dihedral type defined in force field file");
    }
    bts[ebtsCMAP]  = F_CMAP;
    bts[ebtsEXCLS] = 0;

    ftb    = bts[ebtsBONDS];
    nexcl_ = nexcl;
    GenerateComposition(pd);
    if (NAtom() <= 0)
    {
        imm = immAtomTypes;
    }
    if (immOK == imm)
    {
        snew(topology_, 1);
        init_top(topology_);
        /* Get atoms */
        imm = GenerateAtoms(ap, lot, iModel);
    }
    if (immOK == imm)
    {
        /* Store bonds in harmonic potential list first, update type later */
        ftb = F_BONDS;
        memset(&b, 0, sizeof(b));
        for (alexandria::BondIterator bi = BeginBond(); (bi < EndBond()); bi++)
        {
            b.a[0] = bi->GetAi() - 1;
            b.a[1] = bi->GetAj() - 1;
            add_param_to_list(&(plist_[ftb]), &b);
        }
        if (NBond() == 0)
        {
            imm = immGenBonds;
        }
    }
    if (immOK == imm)
    {
        /* Make Angles and Dihedrals. This needs the bonds to be F_BONDS. */
        MakeAngles();

        /* Linear angles and or vsites etc. */
        MakeSpecialInteractions(bUseVsites, pd);

        if (!bPairs)
        {
            /* Check whether this is the right index */
            plist_[F_LJ14].nr = 0;
        }

        if (edih == edihNo)
        {
            /* Check whether this is the right index */
            plist_[F_PDIHS].nr = 0;
        }

        /* Move the plist_ to the correct function */
        mv_plists(pd, plist_, true);
    }
    if (immOK == imm)
    {
        get_force_constants(pd, plist_, &topology_->atoms);

        char **molnameptr = put_symtab(symtab_, GetMolname().c_str());
        snew(mtop_, 1);
        do_init_mtop(pd, mtop_, molnameptr, &topology_->atoms);

        plist_to_mtop(pd, plist_, mtop_);
        excls__to_blocka(topology_->atoms.nr, excls_,
                         &(mtop_->moltype[0].excls));

        ltop_ = gmx_mtop_generate_local_top(mtop_, inputrec_);
    }

    return imm;
}

void MyMol::CalcMultipoles()
{
    int                     i, m;
    rvec                    mu, mm;
    real                    r2, dfac, q;
    gmx_mtop_atomloop_all_t aloop;
    t_atom                 *atom;
    int                     at_global;

    clear_rvec(mu);
    aloop = gmx_mtop_atomloop_all_init(mtop_);
    i     = 0;
    clear_mat(Q_calc);
    clear_rvec(coq);
    while (gmx_mtop_atomloop_all_next(aloop, &at_global, &atom))
    {
        q = atom->q;
        svmul(ENM2DEBYE*q, x_[i], mm);
        rvec_inc(mu, mm);

        dfac = q*0.5*10*ENM2DEBYE;
        r2   = iprod(x_[i], x_[i]);
        for (m = 0; (m < DIM); m++)
        {
            Q_calc[m][m] += dfac*(3*sqr(x_[i][m]) - r2);
        }
        Q_calc[XX][YY] += dfac*3*(x_[i][XX]+coq[XX])*(x_[i][YY]+coq[YY]);
        Q_calc[XX][ZZ] += dfac*3*(x_[i][XX]+coq[XX])*(x_[i][ZZ]+coq[ZZ]);
        Q_calc[YY][ZZ] += dfac*3*(x_[i][YY]+coq[YY])*(x_[i][ZZ]+coq[ZZ]);

        i++;
    }
    gmx_assert(i, topology_->atoms.nr);
    copy_rvec(mu, mu_calc);
    dip_calc = norm(mu);
}

immStatus MyMol::GenerateCharges(gmx_poldata_t pd,
                                 gmx_atomprop_t ap,
                                 ChargeGenerationModel iModel,
                                 real hfac, real epsr,
                                 const char *lot,
                                 bool bSymmetricCharges,
                                 const char *symm_string)
{
    int       i, eQGEN;
    char      qgen_msg[STRLEN];
    immStatus imm = immOK;

    qgen_ = gentop_qgen_init(pd, &topology_->atoms, ap, x_,
                             iModel, hfac, GetCharge(), epsr);

    if (iModel == eqgNone)
    {
        return imm;
    }
    if (immOK == imm)
    {
        if (bSymmetricCharges)
        {
            symmetrize_charges(bSymmetricCharges,
                               &topology_->atoms, &(plist_[F_BONDS]),
                               pd, ap, symm_string, symmetric_charges_);
        }
    }

    if (immOK == imm)
    {
        switch (iModel)
        {
            case eqgRESP:
            case eqgRESPG:
                if (gmx_resp_add_atom_info(gr_, &topology_->atoms, pd))
                {
                    gmx_resp_add_atom_symmetry(gr_, symmetric_charges_);
                    gmx_resp_update_atomtypes(gr_, &(topology_->atoms));
                    gmx_resp_summary(debug, gr_, symmetric_charges_);
                    gmx_resp_add_atom_coords(gr_, x_);
                    /* Even if we get the right LoT it may still not have
                     * the ESP
                     */
                    CalculationIterator ci = GetLotPropType(lot,
                                                            MPO_POTENTIAL,
                                                            NULL);
                    if (ci != EndCalculation())
                    {
                        //printf("There are %d potential points\n",ci->NPotential());
                        for (ElectrostaticPotentialIterator epi = ci->BeginPotential(); (epi < ci->EndPotential()); epi++)
                        {
                            /* Maybe not convert to gmx ? */
                            int xu = string2unit(epi->GetXYZunit().c_str());
                            int vu = string2unit(epi->GetVunit().c_str());
                            if (-1 == xu)
                            {
                                xu = eg2cAngstrom;
                            }
                            if (-1 == vu)
                            {
                                vu = eg2cHartree_e;
                            }
                            gmx_resp_add_point(gr_,
                                               convert2gmx(epi->GetX(), xu),
                                               convert2gmx(epi->GetY(), xu),
                                               convert2gmx(epi->GetZ(), xu),
                                               convert2gmx(epi->GetV(), vu));
                        }
                    }
                }
                break;
            case eqgESP:
                break;
            case eqgNone:
                /* Check which algorithm to use for charge generation */
                strcpy(qgen_msg, "");
                printf("Using zero charges!\n");
                for (i = 0; (i < topology_->atoms.nr); i++)
                {
                    topology_->atoms.atom[i].q  = topology_->atoms.atom[i].qB = 0;
                }
                eQGEN = eQGEN_OK;
                break;
            default:
                if (NULL == qgen_)
                {
                    gmx_fatal(FARGS, "Can not generate charges for %s. Probably due to issues with atomtype detection or support.\n", GetMolname().c_str());
                }
                eQGEN = generate_charges(NULL,
                                         qgen_, NULL, GetMolname().c_str(),
                                         pd, &topology_->atoms, 0.0001,
                                         10000, 1, ap);
                qgen_message(qgen_, sizeof(qgen_msg), qgen_msg, gr_);
                if (eQGEN_OK != eQGEN)
                {
                    imm = immChargeGeneration;
                }
                break;
        }
    }
    return imm;
}

immStatus MyMol::GenerateGromacs(output_env_t oenv, t_commrec *cr)
{
    int nalloc = 2 * topology_->atoms.nr;

    snew(f_, nalloc);
    fr_ = mk_forcerec();
    init_forcerec(NULL, oenv, fr_, NULL, inputrec_, mtop_, cr,
                  box, NULL, NULL, NULL, NULL, NULL, NULL, TRUE, -1);
    // HACK
    fr_->nthreads = 1;

    init_state(&state_, topology_->atoms.nr, 1, 1, 1, 0);
    ltop_ = gmx_mtop_generate_local_top(mtop_, inputrec_);
    md_   = init_mdatoms(NULL, mtop_, FALSE);
    for (int i = 0; (i < topology_->atoms.nr); i++)
    {
        copy_rvec(x_[i], state_.x[i]);
    }
    return immOK;
}

static void put_in_box(int natom, matrix box, rvec x[], real dbox)
{
    int  i, m;
    rvec xmin, xmax, xcom;

    clear_rvec(xcom);
    copy_rvec(x[0], xmin);
    copy_rvec(x[0], xmax);
    for (i = 0; (i < natom); i++)
    {
        rvec_inc(xcom, x[i]);
        for (m = 0; (m < DIM); m++)
        {
            if (xmin[m] > x[i][m])
            {
                xmin[m] = x[i][m];
            }
            else if (xmax[m] < x[i][m])
            {
                xmax[m] = x[i][m];
            }
        }
    }
    for (m = 0; (m < DIM); m++)
    {
        xcom[m]  /= natom;
        box[m][m] = (dbox+xmax[m]-xmin[m]);
    }
}

void MyMol::PrintConformation(const char *fn)
{
    char title[STRLEN];

    put_in_box(topology_->atoms.nr, box, x_, 0.3);
    sprintf(title, "%s processed by %s", GetMolname().c_str(), ShortProgram());
    write_sto_conf(fn, title, &topology_->atoms, x_, NULL, epbcNONE, box);
}

static void write_zeta_q(FILE *fp, gentop_qgen_t qgen,
                         t_atoms *atoms, ChargeGenerationModel iModel)
{
    int    i, ii, j, k, nz, row;
    double zeta, q;
    bool   bAtom, bTypeSet;

    if (NULL == qgen)
    {
        return;
    }

    fprintf(fp, "[ charge_spreading ]\n");
    fprintf(fp, "; This section describes additional atom type properties.\n");
    fprintf(fp, "; Spreading type (stype) can be either Gaussian (AXg) or Slater (AXs).\n");
    fprintf(fp, "; The zeta are the same for atoms of the same type, and all but the last\n");
    fprintf(fp, "; charge as well. The final charge is different between atoms however,\n");
    fprintf(fp, "; and it is listed below in the [ atoms ] section.\n");
    fprintf(fp, "; atype stype  nq%s      zeta          q  ...\n",
            (iModel == eqgAXs) ? "  row" : "");

    k = -1;
    for (i = 0; (i < atoms->nr); i++)
    {
        bAtom = (atoms->atom[i].ptype == eptAtom);
        if (bAtom)
        {
            k++;
        }
        if (k == -1)
        {
            gmx_fatal(FARGS, "The first atom must be a real atom, not a shell");
        }
        nz = gentop_qgen_get_nzeta(qgen, k);
        if (nz != NOTSET)
        {
            bTypeSet = false;
            for (ii = 0; !bTypeSet && (ii < i); ii++)
            {
                bTypeSet = (atoms->atom[ii].type == atoms->atom[i].type);
            }
            if (!bTypeSet)
            {
                fprintf(fp, "%5s %6s %3d",
                        *atoms->atomtype[i],
                        get_eemtype_name(iModel), (bAtom) ? nz : 1);
            }
            for (j = (bAtom ? 0 : nz); (j < (bAtom ? nz : nz)); j++)
            {
                row   = gentop_qgen_get_row(qgen, k, j);
                q     = gentop_qgen_get_q(qgen, k, j);
                zeta  = gentop_qgen_get_zeta(qgen, k, j);
                if ((row != NOTSET) && (q != NOTSET) && (zeta != NOTSET))
                {
                    if (j == nz-1)
                    {
                        atoms->atom[i].q      =
                            atoms->atom[i].qB = q;
                    }
                    if (!bTypeSet)
                    {
                        if (iModel == eqgAXs)
                        {
                            fprintf(fp, "  %4d", row);
                        }
                        fprintf(fp, " %10f", zeta);
                        if (j < nz-1)
                        {
                            fprintf(fp, " %10f", q);
                        }
                    }
                }
            }
            if (!bTypeSet)
            {
                fprintf(fp, "\n");
            }
        }
    }
    fprintf(fp, "\n");
}

static void write_zeta_q2(gentop_qgen_t qgen, gpp_atomtype_t atype,
                          t_atoms *atoms, ChargeGenerationModel iModel)
{
    FILE      *fp;
    int        i, j, k, nz, row;
    double     zeta, q, qtot;
    gmx_bool   bAtom;

    if (NULL == qgen)
    {
        return;
    }

    fp = fopen("zeta_q.txt", "w");
    k  = -1;
    for (i = 0; (i < atoms->nr); i++)
    {
        bAtom = (atoms->atom[i].ptype == eptAtom);
        if (bAtom)
        {
            k++;
        }
        if (k == -1)
        {
            gmx_fatal(FARGS, "The first atom must be a real atom, not a shell");
        }
        nz = gentop_qgen_get_nzeta(qgen, k);
        if (nz != NOTSET)
        {
            fprintf(fp, "%6s  %5s  %5d", get_eemtype_name(iModel),
                    get_atomtype_name(atoms->atom[i].type, atype),
                    (bAtom) ? nz-1 : 1);
            qtot = 0;
            for (j = (bAtom ? 0 : nz-1); (j < (bAtom ? nz-1 : nz)); j++)
            {
                row   = gentop_qgen_get_row(qgen, k, j);
                q     = gentop_qgen_get_q(qgen, k, j);
                zeta  = gentop_qgen_get_zeta(qgen, k, j);
                if ((row != NOTSET) && (q != NOTSET) && (zeta != NOTSET))
                {
                    qtot += q;
                    fprintf(fp, "%5d %10g %10g", row, zeta, q);
                }
            }
            atoms->atom[i].q = qtot;
            fprintf(fp, "\n");
        }
    }
    fprintf(fp, "\n");
    fclose(fp);
}

static void print_bondeds2(FILE     *out,
                           directive d,
                           int       ftype,
                           int       subtype,
                           t_params  p[])
{
    if (p[ftype].nr == 0)
    {
        return;
    }
    fprintf(out, "[ %s ]\n", dir2str(d));
    fprintf(out, ";atom i");
    for (int j = 1; (j < NRAL(ftype)); j++)
    {
        fprintf(out, "  %5c", j+'i');
    }
    fprintf(out, "   type  parameters\n");

    for (int i = 0; (i < p[ftype].nr); i++)
    {
        for (int j = 0; (j < NRAL(ftype)); j++)
        {
            fprintf(out, "  %5d", 1+p[ftype].param[i].a[j]);
        }
        fprintf(out, "  %5d", 1+subtype);
        for (int j = 0; (j < NRFPA(ftype)); j++)
        {
            fprintf(out, "  %10g", p[ftype].param[i].c[j]);
        }
        fprintf(out, "\n");
    }
    fprintf(out, "\n");
}

static void write_top2(FILE *out, char *molname,
                       t_atoms *at, gmx_bool bRTPresname,
                       int bts[], t_params plist_[], t_excls excls[],
                       gpp_atomtype_t atype, int *cgnr, int nrexcl)
/* NOTE: nrexcl is not the size of *excl! */
{
    if (at && atype && cgnr)
    {
        fprintf(out, "[ %s ]\n", dir2str(d_moleculetype));
        fprintf(out, "; %-15s %5s\n", "Name", "nrexcl");
        fprintf(out, "%-15s %5d\n\n", molname ? molname : "Protein", nrexcl);

        print_atoms(out, atype, at, cgnr, bRTPresname);
        print_bondeds2(out, d_bonds,    F_MORSE,  bts[ebtsBONDS],   plist_);
        print_bondeds2(out, d_constraints, F_CONSTR,   0,              plist_);
        print_bondeds2(out, d_constraints, F_CONSTRNC, 0,              plist_);
        print_bondeds2(out, d_pairs,      F_LJ14,     0,              plist_);
        print_excl(out, at->nr, excls);
        print_bondeds2(out, d_angles,     bts[ebtsANGLES], 0, plist_);
        print_bondeds2(out, d_dihedrals,  bts[ebtsPDIHS], 0, plist_);
        print_bondeds2(out, d_dihedrals,  bts[ebtsIDIHS], 0, plist_);
        print_bondeds2(out, d_cmap,       bts[ebtsCMAP],  0, plist_);
        print_bondeds2(out, d_polarization, F_POLARIZATION,   0,       plist_);
        print_bondeds2(out, d_thole_polarization, F_THOLE_POL, 0,       plist_);
        print_bondeds2(out, d_vsites2,    F_VSITE2,   0,              plist_);
        print_bondeds2(out, d_vsites3,    F_VSITE3,   0,              plist_);
        print_bondeds2(out, d_vsites3,    F_VSITE3FD, 0,              plist_);
        print_bondeds2(out, d_vsites3,    F_VSITE3FAD, 0,              plist_);
        print_bondeds2(out, d_vsites3,    F_VSITE3OUT, 0,              plist_);
        print_bondeds2(out, d_vsites4,    F_VSITE4FD, 0,              plist_);
        print_bondeds2(out, d_vsites4,    F_VSITE4FDN, 0,             plist_);

    }
}

void MyMol::PrintTopology(const char           *fn,
                          ChargeGenerationModel iModel,
                          bool                  bVerbose)
{
    FILE   *fp;
    t_mols  printmol;
    bool    bITP;
    int     i, bts2[ebtsNR];
    if (GetMolname().size() > 0)
    {
        printmol.name = strdup(GetMolname().c_str());
    }
    else if (GetFormula().size() > 0)
    {
        printmol.name = strdup(GetFormula().c_str());
    }
    else
    {
        printmol.name = strdup("Onbekend");
    }
    printmol.nr   = 1;

    /* Write topology_ file */
    bITP = (fn2ftp(fn) == efITP);
    fp   = gmx_ffopen(fn, "w");
    if (!bITP)
    {
        std::string ff = GetForceField();
        ff.append(".ff");
        print_top_header(fp, fn, bITP, ff.c_str(), 1.0, "Check your output");
    }

    /* Make pdb2gmx compatible bts array
     * This is to write a topology_ that grompp can read again.
     */
    for (i = 0; (i < ebtsNR); i++)
    {
        bts2[i] = NOTSET;
    }
    for (i = 1; (i < 20) && (bts2[ebtsBONDS] == NOTSET); i++)
    {
        if (ifunc_index(d_bonds, i) == bts[ebtsBONDS])
        {
            bts2[ebtsBONDS] = i;
        }
    }
    for (i = 1; (i < 20) && (bts2[ebtsANGLES] == NOTSET); i++)
    {
        if (ifunc_index(d_angles, i) == bts[ebtsANGLES])
        {
            bts2[ebtsANGLES] = i;
        }
    }
    for (i = 1; (i < 20) && (bts2[ebtsPDIHS] == NOTSET); i++)
    {
        if (ifunc_index(d_dihedrals, i) == bts[ebtsPDIHS])
        {
            bts2[ebtsPDIHS] = i;
        }
    }
    for (i = 1; (i < 20) && (bts2[ebtsIDIHS] == NOTSET); i++)
    {
        if (ifunc_index(d_dihedrals, i) == bts[ebtsIDIHS])
        {
            bts2[ebtsIDIHS] = i;
        }
    }
    bts2[ebtsEXCLS] = 0;
    bts2[ebtsCMAP]  = 0;
    for (i = 0; (i < ebtsNR); i++)
    {
        if (NOTSET == bts2[i])
        {
            gmx_fatal(FARGS, "Could not find ftype for bts[%d]", i);
        }
    }


    if (bHaveShells_ || (iModel == eqgAXg) || (iModel == eqgAXs))
    {
        write_zeta_q(fp, qgen_, &topology_->atoms, iModel);
        //write_zeta_q2(qgen,atype,&topology_->atoms,pd,iModel);
    }
    // mv_plist_s(pd,plist_,false);
    write_top2(fp, printmol.name, &topology_->atoms, FALSE, bts, plist_, excls_, atype_, cgnr_, nexcl_);
    // mv_plist_s(pd,plist_,true);
    if (!bITP)
    {
        print_top_mols(fp, printmol.name, GetForceField().c_str(), NULL, 0, NULL, 1, &printmol);
    }

    if (bVerbose)
    {
        printf("There are %4d proper dihedrals, %4d impropers\n"
               "          %4d angles, %4d linear angles\n"
               "          %4d pairs, %4d bonds, %4d atoms\n"
               "          %4d polarizations\n",
               plist_[bts[ebtsPDIHS]].nr,  plist_[bts[ebtsIDIHS]].nr,
               plist_[bts[ebtsANGLES]].nr, plist_[F_LINEAR_ANGLES].nr,
               plist_[F_LJ14].nr,   plist_[bts[ebtsBONDS]].nr, topology_->atoms.nr,
               plist_[F_POLARIZATION].nr);
    }

    fclose(fp);
}

void MyMol::PrintRTPEntry(const char *fn)
{
    print_rtp(fn, gentop_version,
              &topology_->atoms, plist_, cgnr_, asize(bts), bts);
}

static void add_excl(t_excls *excls, atom_id e)
{
    int i;

    for (i = 0; (i < excls->nr); i++)
    {
        if (excls->e[i] == e)
        {
            return;
        }
    }
    srenew(excls->e, excls->nr+1);
    excls->e[excls->nr++] = e;
}

static void remove_excl(t_excls *excls, int remove)
{
    int i;

    for (i = remove+1; i < excls->nr; i++)
    {
        excls->e[i-1] = excls->e[i];
    }

    excls->nr--;
}

static void prune_excl(t_excls excls[], t_atoms *atoms, gpp_atomtype_t atype)
{
    int i, k, ak;

    for (i = 0; (i < atoms->nr); i++)
    {
        if (get_atomtype_ptype(atoms->atom[i].type, atype) != eptShell)
        {
            for (k = 0; (k < excls[i].nr); )
            {
                ak = excls[i].e[k];
                if (get_atomtype_ptype(atoms->atom[ak].type, atype) != eptShell)
                {
                    remove_excl(&(excls[i]), k);
                }
                else
                {
                    k++;
                }
            }
        }
    }
}

static void copy_atoms(t_atoms *src, t_atoms *dest)
{
    int i;

    if (dest->nr < src->nr)
    {
        srenew(dest->atom, src->nr);
        srenew(dest->atomname, src->nr);
        if (NULL != src->atomtype)
        {
            srenew(dest->atomtype, src->nr);
        }
        else if (NULL != dest->atomtype)
        {
            sfree(dest->atomtype);
            dest->atomtype = NULL;
        }
        if (NULL != src->atomtypeB)
        {
            srenew(dest->atomtypeB, src->nr);
        }
        else if (NULL != dest->atomtypeB)
        {
            sfree(dest->atomtypeB);
            dest->atomtypeB = NULL;
        }
    }
    dest->nr = src->nr;
    for (i = 0; (i < src->nr); i++)
    {
        dest->atom[i]      = src->atom[i];
        dest->atomname[i]  = src->atomname[i];
        if (NULL != src->atomtype)
        {
            dest->atomtype[i]  = src->atomtype[i];
        }
        if (NULL != src->atomtypeB)
        {
            dest->atomtypeB[i] = src->atomtypeB[i];
        }
    }
    if (dest->nres < src->nres)
    {
        srenew(dest->resinfo, src->nres);
    }

    if (NULL != src->pdbinfo)
    {
        srenew(dest->pdbinfo, src->nres);
    }
    else if (NULL != dest->pdbinfo)
    {
        sfree(dest->pdbinfo);
        dest->pdbinfo = NULL;
    }
    dest->nres = src->nres;
    for (i = 0; (i < src->nres); i++)
    {
        dest->resinfo[i] = src->resinfo[i];
        if (NULL != src->pdbinfo)
        {
            dest->pdbinfo[i] = src->pdbinfo[i];
        }
    }
}

void MyMol::AddShells(gmx_poldata_t pd, ePolar epol)
{
    int      i, j, k, ai, aj, iat, shell, ns = 0;
    int     *renum;
    char     buf[32], **newname;
    t_param  p;
    t_atom  *shell_atom;
    t_atoms *newa;
    t_excls *newexcls;
    rvec    *newx;
    double   pol, sigpol;

    int      maxatom = topology_->atoms.nr*2+2;
    srenew(x_, maxatom);
    srenew(excls_, maxatom);
    snew(shell_atom, 1);
    shell_atom->ptype = eptShell;
    memset(&p, 0, sizeof(p));
    snew(renum, maxatom);
    for (i = 0; (i < topology_->atoms.nr); i++)
    {
        renum[i] = i+ns;
        if (1 == gmx_poldata_get_atype_pol(pd, *topology_->atoms.atomtype[i],
                                           &pol, &sigpol))
        {
            ns++;
            p.a[0] = renum[i];
            p.a[1] = renum[i]+1;
            p.c[0] = 0.001*pol;
            add_param_to_list(&(plist_[F_POLARIZATION]), &p);
        }
    }
    renum[topology_->atoms.nr] = topology_->atoms.nr + ns;
    printf("added %d shells\n", ns);
    if (ns > 0)
    {
        /* Make new atoms and x arrays */
        snew(newa, 1);
        init_t_atoms(newa, topology_->atoms.nr+ns, TRUE);
        snew(newa->atomtype, topology_->atoms.nr+ns);
        snew(newa->atomtypeB, topology_->atoms.nr+ns);
        newa->nres = topology_->atoms.nres;
        snew(newx, newa->nr);
        snew(newname, newa->nr);

        /* Make new exclusion array, and put the shells in it */
        snew(newexcls, newa->nr);
        for (j = 0; (j < plist_[F_POLARIZATION].nr); j++)
        {
            ai = plist_[F_POLARIZATION].param[j].a[0];
            aj = plist_[F_POLARIZATION].param[j].a[1];
            add_excl(&newexcls[ai], aj);
            add_excl(&newexcls[aj], ai);
        }
        for (i = 0; (i < topology_->atoms.nr); i++)
        {
            newa->atom[renum[i]]      = topology_->atoms.atom[i];
            newa->atomname[renum[i]]  = put_symtab(symtab_, *topology_->atoms.atomname[i]);
            newa->atomtype[renum[i]]  = put_symtab(symtab_, *topology_->atoms.atomtype[i]);
            newa->atomtypeB[renum[i]] = put_symtab(symtab_, *topology_->atoms.atomtypeB[i]);
            copy_rvec(x_[i], newx[renum[i]]);
            newname[renum[i]] = *topology_->atoms.atomtype[i];
            t_atoms_set_resinfo(newa, renum[i], symtab_,
                                *topology_->atoms.resinfo[topology_->atoms.atom[i].resind].name,
                                topology_->atoms.atom[i].resind, ' ', 1, ' ');
        }

        for (i = 0; (i < topology_->atoms.nr); i++)
        {
            iat = renum[i];
            for (k = 0; (k < excls_[i].nr); k++)
            {
                add_excl(&(newexcls[iat]), renum[excls_[i].e[k]]);
            }
            for (j = iat+1; (j < renum[i+1]); j++)
            {
                newa->atom[j]            = topology_->atoms.atom[i];
                newa->atom[iat].q        = 0;
                newa->atom[iat].qB       = 0;
                newa->atom[j].m          = 0;
                newa->atom[j].mB         = 0;
                newa->atom[j].atomnumber = 0;
                sprintf(buf, "%ss", get_atomtype_name(topology_->atoms.atom[i].type,
                                                      atype_));
                newname[j] = strdup(buf);
                shell      = add_atomtype(atype_, symtab_, shell_atom, buf, &p,
                                          0, 0, 0, 0, 0, 0, 0);
                newa->atom[j].type      = shell;
                newa->atom[j].typeB     = shell;
                newa->atomtype[j]       =
                    newa->atomtypeB[j]  = put_symtab(symtab_, buf);
                newa->atom[j].ptype     = eptShell;
                newa->atom[j].resind    = topology_->atoms.atom[i].resind;
                sprintf(buf, "%ss", *(topology_->atoms.atomname[i]));
                newa->atomname[j] = put_symtab(symtab_, buf);
                copy_rvec(x_[i], newx[j]);
                for (k = 0; (k < excls_[i].nr); k++)
                {
                    ai = j;
                    aj = renum[excls_[i].e[k]];
                    if (ai != aj)
                    {
                        add_excl(&(newexcls[ai]), aj);
                        add_excl(&(newexcls[aj]), ai);
                    }
                }
            }
        }
        for (i = 0; (i < topology_->atoms.nr); i++)
        {
            iat = renum[i];
            for (j = iat+1; (j < renum[i+1]); j++)
            {
                for (k = 0; (k < newexcls[iat].nr); k++)
                {
                    ai = j;
                    aj = newexcls[iat].e[k];
                    if (ai != aj)
                    {
                        add_excl(&(newexcls[ai]), aj);
                        add_excl(&(newexcls[aj]), ai);
                    }
                }
            }
        }
        prune_excl(newexcls, newa, atype_);
        /* Copy newa to atoms */
        copy_atoms(newa, &topology_->atoms);
        /* Copy coordinates and smnames */
        for (i = 0; (i < newa->nr); i++)
        {
            copy_rvec(newx[i], x_[i]);
            topology_->atoms.atomtype[i] = put_symtab(symtab_, newname[i]);
        }
        sfree(newx);
        sfree(newname);
        /* Copy exclusions, may need to empty the original first */
        excls_ = newexcls;

        for (i = 0; (i < F_NRE); i++)
        {
            if (i != F_POLARIZATION)
            {
                for (j = 0; (j < plist_[i].nr); j++)
                {
                    for (k = 0; (k < NRAL(i)); k++)
                    {
                        plist_[i].param[j].a[k] = renum[plist_[i].param[j].a[k]];
                    }
                }
            }
        }
    }
    sfree(renum);
    sfree(shell_atom);
}

immStatus MyMol::GenerateChargeGroups(eChargeGroup ecg, bool bUsePDBcharge,
                                      const char *ndxfn, int nmol)
{
    real qtot, mtot;

    if ((cgnr_ = generate_charge_groups(ecg, &topology_->atoms,
                                        &plist_[bts[ebtsBONDS]], &plist_[F_POLARIZATION],
                                        bUsePDBcharge,
                                        &qtot, &mtot)) == NULL)
    {
        return immChargeGeneration;
    }

    if (ecg != ecgAtom)
    {
        sort_on_charge_groups(cgnr_, &topology_->atoms, plist_, x_, excls_, ndxfn, nmol);
    }
    return immOK;
}

void MyMol::GenerateCube(ChargeGenerationModel iModel,
                         gmx_poldata_t         pd,
                         real                  spacing,
                         const char           *reffn,
                         const char           *pcfn,
                         const char           *pdbdifffn,
                         const char           *potfn,
                         const char           *rhofn,
                         const char           *hisfn,
                         const char           *difffn,
                         const char           *diffhistfn,
                         output_env_t          oenv)
{
    char       *gentop_version = (char *)"v0.99b";
    gmx_resp_t  grref;

    if (NULL != gr_)
    {
        /* This has to be done before the grid is f*cked up by
           writing a cube file */
        grref = gmx_resp_copy(gr_);
        gmx_resp_potcomp(gr_, pcfn, pdbdifffn, oenv);
        if ((NULL != potfn) || (NULL != hisfn) || (NULL != rhofn) ||
            ((NULL != difffn) && (NULL != reffn)))
        {
            char buf[256];

            sprintf(buf, "Potential generated by %s based on %s charges",
                    gentop_version,
                    get_eemtype_name(iModel));

            if (NULL != difffn)
            {
                gmx_resp_add_atom_info(grref, &topology_->atoms, pd);
                gmx_resp_add_atom_symmetry(grref, symmetric_charges_);
                gmx_resp_read_cube(grref, reffn, FALSE);
                gmx_resp_copy_grid(gr_, grref);
            }
            else
            {
                gmx_resp_make_grid(gr_, spacing, box, x_);
            }
            if (NULL != rhofn)
            {
                sprintf(buf, "Electron density generated by %s based on %s charges",
                        gentop_version, get_eemtype_name(iModel));
                gmx_resp_calc_rho(gr_);
                gmx_resp_write_rho(gr_, rhofn, buf);
            }
            sprintf(buf, "Potential generated by %s based on %s charges",
                    gentop_version, get_eemtype_name(iModel));
            if (NULL != potfn)
            {
                gmx_resp_calc_pot(gr_);
                gmx_resp_write_cube(gr_, potfn, buf);
            }
            if (NULL != hisfn)
            {
                gmx_resp_write_histo(gr_, hisfn, buf, oenv);
            }
            if ((NULL != difffn) || (NULL != diffhistfn))
            {
                sprintf(buf, "Potential difference generated by %s based on %s charges",
                        gentop_version,
                        get_eemtype_name(iModel));
                gmx_resp_write_diff_cube(grref, gr_, difffn, diffhistfn, buf, oenv, 0);
                gmx_resp_destroy(grref);
            }
        }
        gmx_resp_destroy(grref);
    }
}

immStatus MyMol::GetExpProps(gmx_bool bQM, gmx_bool bZero, char *lot,
                             alexandria::GaussAtomProp &gap)
{
    immStatus    imm = immOK;
    unsigned int m, nwarn = 0;
    double       value, dv0, dv298, error, vec[3];
    tensor       quadrupole;
    char        *myref, *mylot;
    int          ia;

    if (GetPropRef(MPO_DIPOLE, (bQM ? iqmQM : iqmBoth),
                   lot, NULL, (char *)"elec",
                   &value, &error, &myref, &mylot,
                   vec, quadrupole))
    {
        if (!bZero)
        {
            imm = immZeroDip;
        }
        if (NULL != myref)
        {
            sfree(myref);
        }
        if (NULL != mylot)
        {
            sfree(mylot);
        }
    }
    else
    {
        dip_exp  = value;
        dip_err  = error;
        lot      = mylot;
        //ref      = myref;
        for (m = 0; (m < DIM); m++)
        {
            mu_exp[m] = vec[m];
        }
        mu_exp2 = sqr(value);
        if (error <= 0)
        {
            if (debug)
            {
                fprintf(debug, "WARNING: Error for %s is %g, assuming it is 10%%.\n",
                        GetMolname().c_str(), error);
            }
            nwarn++;
            error = 0.1*value;
        }
        dip_weight = sqr(1.0/error);
    }
    if (GetPropRef(MPO_DIPOLE, iqmQM,
                   lot, NULL, (char *)"ESP", &value, &error, NULL, NULL, vec, quadrupole))
    {
        for (m = 0; (m < DIM); m++)
        {
            mu_esp[m] = vec[m];
        }
    }
    if (GetProp(MPO_ENERGY, (bQM ? iqmQM : iqmBoth),
                lot, NULL, (char *)"DHf(298.15K)", &value, NULL))
    {
        Hform = value;
        Emol  = value;
        for (ia = 0; (ia < topology_->atoms.nr); ia++)
        {
            if (gap.GetValue(*topology_->atoms.atomname[ia],
                             (char *)"exp", (char *)"DHf(0K)", 0, &dv0) &&
                gap.GetValue(*topology_->atoms.atomname[ia],
                             (char *)"exp", (char *)"H(0K)-H(298.15K)",
                             298.15, &dv298))
            {
                Emol -= convert2gmx(dv0+dv298, eg2cHartree);
            }
            else
            {
                Emol = 0;
                break;
            }
        }
        if (ia < topology_->atoms.nr)
        {
            imm = immNoData;
        }
    }
    else
    {
        imm = immNoData;
    }
    return imm;
}

void MyMol::PrintQPol(FILE *fp, gmx_poldata_t pd)
{
    int     i, m, np;
    double  poltot, pol, sigpol, sptot;
    char   *gt_type;
    rvec    mu;

    poltot = 0;
    sptot  = 0;
    np     = 0;
    clear_rvec(mu);
    for (i = 0; (i < topology_->atoms.nr); i++)
    {
        if (1 == 
            gmx_poldata_get_atype_pol(pd, *topology_->atoms.atomtype[i], &pol, &sigpol))
        {
            np++;
            poltot += pol;
            sptot  += sqr(sigpol);
        }
        for (m = 0; (m < DIM); m++)
        {
            mu[m] += x_[i][m]*topology_->atoms.atom[i].q;
        }
    }
    int    qq    = GetCharge();
    double mm    = GetMass();
    double mutot = ENM2DEBYE*norm(mu);
    fprintf(fp, "Total charge is %d, total mass is %g, dipole is %f D\n",
            qq, mm, mutot);
    fprintf(fp, "Polarizability is %g +/- %g A^3.\n", poltot, sqrt(sptot/topology_->atoms.nr));
}

void MyMol::UpdateIdef(gmx_poldata_t pd, bool bOpt[])
{
    int    gt, i, tp, ai, aj, ak, al;
    int    ftb, fta, ftd;
    char  *aai, *aaj, *aak, *aal, *params;
    int    lu;
    double value;

    lu = string2unit(gmx_poldata_get_length_unit(pd));
    if (bOpt[ebtsBONDS])
    {
        ftb = gmx_poldata_get_bond_ftype(pd);
        for (i = 0; (i < ltop_->idef.il[ftb].nr); i += interaction_function[ftb].nratoms+1)
        {
            tp  = ltop_->idef.il[ftb].iatoms[i];
            ai  = ltop_->idef.il[ftb].iatoms[i+1];
            aj  = ltop_->idef.il[ftb].iatoms[i+2];
            aai = (char *)gmx_poldata_atype_to_btype(pd, *topology_->atoms.atomtype[ai]);
            aaj = (char *)gmx_poldata_atype_to_btype(pd, *topology_->atoms.atomtype[aj]);
            /* Here unfortunately we need a case statement for the types */
            if ((gt = gmx_poldata_search_bond(pd, aai, aaj, &value, NULL, NULL, NULL, &params)) != 0)
            {
                mtop_->ffparams.iparams[tp].morse.b0A = convert2gmx(value, lu);

                std::vector<std::string> ptr = split(params, ' ');
                int n = 0;
                for (std::vector<std::string>::iterator pi = ptr.begin(); (pi < ptr.end()); ++pi)
                {
                    if (pi->length() > 0)
                    {
                        if (n == 0)
                        {
                            mtop_->ffparams.iparams[tp].morse.cbA = atof(pi->c_str());
                        }
                        else
                        {
                            mtop_->ffparams.iparams[tp].morse.betaA = atof(pi->c_str());
                        }
                        n++;
                    }
                }
                if (NULL != params)
                {
                    sfree(params);
                }
            }
            else
            {
                gmx_fatal(FARGS, "There are no parameters for bond %s-%s in the force field", aai, aaj);
            }
        }
    }
    if (bOpt[ebtsANGLES])
    {
        fta = gmx_poldata_get_angle_ftype(pd);
        for (i = 0; (i < ltop_->idef.il[fta].nr); i += interaction_function[fta].nratoms+1)
        {
            tp  = ltop_->idef.il[fta].iatoms[i];
            ai  = ltop_->idef.il[fta].iatoms[i+1];
            aj  = ltop_->idef.il[fta].iatoms[i+2];
            ak  = ltop_->idef.il[fta].iatoms[i+3];
            aai = (char *)gmx_poldata_atype_to_btype(pd, *topology_->atoms.atomtype[ai]);
            aaj = (char *)gmx_poldata_atype_to_btype(pd, *topology_->atoms.atomtype[aj]);
            aak = (char *)gmx_poldata_atype_to_btype(pd, *topology_->atoms.atomtype[ak]);

            if ((gt = gmx_poldata_search_angle(pd, aai, aaj, aak, &value,
                                               NULL, NULL, &params)) != 0)
            {
                mtop_->ffparams.iparams[tp].harmonic.rA     =
                    mtop_->ffparams.iparams[tp].harmonic.rB = value;
                std::vector<std::string> ptr = split(params, ' ');
                for (std::vector<std::string>::iterator pi = ptr.begin(); (pi < ptr.end()); ++pi)
                {
                    if (pi->length() > 0)
                    {
                        mtop_->ffparams.iparams[tp].harmonic.krA     =
                            mtop_->ffparams.iparams[tp].harmonic.krB = atof(pi->c_str());
                    }
                }
                if (NULL != params)
                {
                    sfree(params);
                }
            }
            else
            {
                gmx_fatal(FARGS, "There are no parameters for angle %s-%s-%s in the force field", aai, aaj, aak);
            }
        }
    }
    if (bOpt[ebtsPDIHS])
    {
        ftd = gmx_poldata_get_dihedral_ftype(pd, egdPDIHS);

        for (i = 0; (i < ltop_->idef.il[ftd].nr); i += interaction_function[ftd].nratoms+1)
        {
            tp  = ltop_->idef.il[ftd].iatoms[i];
            ai  = ltop_->idef.il[ftd].iatoms[i+1];
            aj  = ltop_->idef.il[ftd].iatoms[i+2];
            ak  = ltop_->idef.il[ftd].iatoms[i+3];
            al  = ltop_->idef.il[ftd].iatoms[i+4];
            aai = (char *)gmx_poldata_atype_to_btype(pd, *topology_->atoms.atomtype[ai]);
            aaj = (char *)gmx_poldata_atype_to_btype(pd, *topology_->atoms.atomtype[aj]);
            aak = (char *)gmx_poldata_atype_to_btype(pd, *topology_->atoms.atomtype[ak]);
            aal = (char *)gmx_poldata_atype_to_btype(pd, *topology_->atoms.atomtype[al]);
            if ((gt = gmx_poldata_search_dihedral(pd, egdPDIHS, aai, aaj, aak, aal,
                                                  &value, NULL, NULL, &params)) != 0)
            {
                mtop_->ffparams.iparams[tp].pdihs.phiA = value;
                std::vector<std::string> ptr = split(params, ' ');
                int n = 0;
                for (std::vector<std::string>::iterator pi = ptr.begin(); (pi < ptr.end()); ++pi)
                {
                    if (pi->length() > 0)
                    {
                        if (n == 0)
                        {
                            mtop_->ffparams.iparams[tp].pdihs.cpA     =
                                mtop_->ffparams.iparams[tp].pdihs.cpB =
                                    atof(pi->c_str());
                        }
                        else
                        {
                            mtop_->ffparams.iparams[tp].pdihs.mult = atof(pi->c_str());
                        }
                        n++;
                    }
                }
                if (NULL != params)
                {
                    sfree(params);
                }
            }
            else
            {
                gmx_fatal(FARGS, "There are no parameters for angle %s-%s-%s in the force field", aai, aaj, aak);
            }
        }
    }
    if (bOpt[ebtsIDIHS])
    {
        ftd = gmx_poldata_get_dihedral_ftype(pd, egdIDIHS);

        for (i = 0; (i < ltop_->idef.il[ftd].nr); i += interaction_function[ftd].nratoms+1)
        {
            tp  = ltop_->idef.il[ftd].iatoms[i];
            ai  = ltop_->idef.il[ftd].iatoms[i+1];
            aj  = ltop_->idef.il[ftd].iatoms[i+2];
            ak  = ltop_->idef.il[ftd].iatoms[i+3];
            al  = ltop_->idef.il[ftd].iatoms[i+4];
            aai = (char *)gmx_poldata_atype_to_btype(pd, *topology_->atoms.atomtype[ai]);
            aaj = (char *)gmx_poldata_atype_to_btype(pd, *topology_->atoms.atomtype[aj]);
            aak = (char *)gmx_poldata_atype_to_btype(pd, *topology_->atoms.atomtype[ak]);
            aal = (char *)gmx_poldata_atype_to_btype(pd, *topology_->atoms.atomtype[al]);
            if ((gt = gmx_poldata_search_dihedral(pd, egdIDIHS, aai, aaj, aak, aal,
                                                  &value, NULL, NULL, &params)) != 0)
            {
                mtop_->ffparams.iparams[tp].harmonic.rA     =
                    mtop_->ffparams.iparams[tp].harmonic.rB = value;
                std::vector<std::string>           ptr = split(params, ' ');
                std::vector<std::string>::iterator pi  = ptr.begin();
                if (pi->length() > 0)
                {
                    mtop_->ffparams.iparams[tp].harmonic.krA     =
                        mtop_->ffparams.iparams[tp].harmonic.krB = atof(pi->c_str());
                }
                if (NULL != params)
                {
                    sfree(params);
                }
            }
            else
            {
                gmx_fatal(FARGS, "There are no parameters for improper %-%s-%s-%s in the force field for %s",
                          aai, aaj, aak, aal, GetMolname().c_str());
            }
        }
    }
}

}
