#include "DeformationGridModel.h"
#include "GenericImageData.h"
#include "GlobalUIModel.h"
#include "SNAPAppearanceSettings.h"
#include <itkImageLinearConstIteratorWithIndex.h>

DeformationGridVertex
::DeformationGridVertex()
{

}

DeformationGridVertex
::DeformationGridVertex(double x, double y)
  : x(x), y(y)
{

}


DeformationGridModel
::DeformationGridModel()
{

}

int
DeformationGridModel::
GetVertices(const ViewportType &vp, DeformationGridVertices &v) const
{
  auto layer = m_Parent->GetImageData()->FindLayer(vp.layer_id, false);
  assert(layer); // if failed, find out why this viewport keeps a non-exist layer_id

  // Draw the texture for the layer
  AnatomicImageWrapper *vecimg = dynamic_cast<AnatomicImageWrapper *>(layer);

  if (vecimg && vecimg->GetNumberOfComponents() == 3)
    {
    // Get the slice
    AnatomicImageWrapper::SliceType::Pointer slice = vecimg->GetSlice(m_Parent->GetId());
    slice->GetSource()->UpdateLargestPossibleRegion();

    // Appearance settings for grid lines
    SNAPAppearanceSettings *as = m_Parent->GetParentUI()->GetAppearanceSettings();
    const OpenGLAppearanceElement *elt =
        as->GetUIElement(SNAPAppearanceSettings::GRID_LINES);

    // Line properties
    //elt->ApplyLineSettings();

    // The mapping between (index, phi[index]) and on-screen coordinate for a grid
    // point is linear (combines a bunch of transforms). To save time, we can
    // compute this mapping once at the beginning of the loop. We also know that the
    // index will only be going up by one at each iteration
    itk::Index<2> ind;
    Vector3d phi, G0, d_grid_d_phi[3], d_grid_d_ind[2];

    // Compute the initial displacement G0
    ind.Fill(0); phi.fill(0.0f);
    G0 = m_Parent->ComputeGridPosition(phi, ind, vecimg);

    // Compute derivative of grid displacement wrt warp components
    for(int a = 0; a < 3; a++)
      {
      ind.Fill(0); phi.fill(0.0f);
      phi[a] = 1.0f;
      d_grid_d_phi[a] = m_Parent->ComputeGridPosition(phi, ind, vecimg) - G0;
      }

    // Compute derivative of grid displacement wrt index components
    for(int b = 0; b < 2; b++)
      {
      ind.Fill(0); phi.fill(0.0f);
      ind[b] = 1;
      d_grid_d_ind[b] = m_Parent->ComputeGridPosition(phi, ind, vecimg) - G0;
      }

    size_t nd0[2] {0, 0}, nd1[2] {0, 0};
    bool counted = false;

    // Iterate line direction
    for(int d = 0; d < 2; d++)
      {
      // The current matrix is such that we should be drawing in pixel coordinates.
      typedef itk::ImageLinearConstIteratorWithIndex<AnatomicImageWrapper::SliceType> IterType;
      IterType it1(slice, slice->GetBufferedRegion());
      it1.SetDirection(d);
      it1.GoToBegin();

      auto d0 = it1.GetRegion().GetSize()[0];
      auto d1 = it1.GetRegion().GetSize()[1];

      int vox_increment;
      if(vecimg->IsSlicingOrthogonal())
        {
        // Figure out how frequently to sample lines. The spacing on the screen should be at
        // most every 4 pixels. Zoom is in units of px/mm. Spacing is in units of mm/vox, so
        // zoom * spacing is (display pixels) / (image voxels).
        double disp_pix_per_vox = m_Parent->GetSliceSpacing()[d] * m_Parent->GetViewZoom();
        vox_increment = (int) ceil(8.0 / disp_pix_per_vox);
        }
      else
        {
        // The slice is in screen pixel units already - so just 8!
        vox_increment = 8;
        }

      std::cout << "[DGModel] d0 = " << d0 << "; n0 = " << ceil(d0/vox_increment)
                << "; d1 = " << d1 << "; n1 = " << ceil(d1/vox_increment)
                << std::endl;

      counted = false;
      while( !it1.IsAtEnd() )
        {
        // Do we draw this line?
        if(it1.GetIndex()[1-d] % vox_increment == 0)
          {
          ++nd0[d];

          // Set up the current position and increment
          Vector3d G1 = G0 +
              (d_grid_d_ind[0] * (double) (it1.GetIndex()[0])) +
              (d_grid_d_ind[1] * (double) (it1.GetIndex()[1]));

          while( !it1.IsAtEndOfLine() )
            {
            if (!counted)
              ++nd1[d];

            // Read the pixel
            AnatomicImageWrapper::SliceType::PixelType pix = it1.Get();

            // Alternative version
            Vector3d xDispSlice = G1 +
                (d_grid_d_phi[0] * (double) (pix[0])) +
                (d_grid_d_phi[1] * (double) (pix[1])) +
                (d_grid_d_phi[2] * (double) (pix[2]));

            //v.vlist[d].push_back(DeformationGridVertex(xDispSlice[0], xDispSlice[1]));
            v.vvec.push_back(xDispSlice[0]);
            v.vvec.push_back(xDispSlice[1]);

            // Add the displacement
            ++it1;

            // Update the current position
            G1 += d_grid_d_ind[d];
            }
          counted = true;
          }

        it1.NextLine();
        }
      }

    v.d0_nline = nd0[0];
    v.d0_nvert = nd1[0];
    v.d1_nline = nd0[1];
    v.d1_nvert = nd1[1];

    }



  return EXIT_SUCCESS;
}
